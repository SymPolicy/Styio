#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ipaddress
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = Path(__file__).resolve().with_suffix(".policy.json")
POLICY_REL = (Path("scripts") / POLICY_PATH.name).as_posix()
SUPPRESSION_TOKEN = "local-info: allow"

# Raw byte inputs under these prefixes are fuzz seeds, not publishable text.
# They are counted as skipped-by-policy rather than scanned.
BINARY_SKIP_PREFIXES = ("tests/fuzz/corpus/",)

WINDOWS_ABSOLUTE_PATH_RE = re.compile(
    r"(?<![A-Za-z0-9_])(?:[A-Za-z]:[\\/][^\s`'\"<>|)]+)"
)
POSIX_USER_PATH_RE = re.compile(
    r"(?<![$A-Za-z0-9_])/(?:home|Users)/[A-Za-z0-9._-]+(?:/[^\s`'\"<>)]*)?"
)
SERVER_PATH_RE = re.compile(
    r"(?<![$A-Za-z0-9_])/(?:srv|var/www)(?:/[^\s`'\"<>)]*)?"
)
SSH_TARGET_RE = re.compile(
    r"(?<![A-Za-z0-9._%+-])(?:ssh://[A-Za-z0-9._-]+@[A-Za-z0-9._-]+/|[A-Za-z0-9._-]+@[A-Za-z0-9._-]+\.[A-Za-z]{2,}:)"
)
UNC_PATH_RE = re.compile(r"\\\\(?!<)[A-Za-z0-9._-]{2,}\\[A-Za-z0-9._$-]{2,}(?:\\[^\s`'\"<>)]*)?")
IPV4_RE = re.compile(r"\b(?:[0-9]{1,3}\.){3}[0-9]{1,3}\b")


@dataclass(frozen=True)
class Policy:
    allowed_ips: frozenset
    documentation_networks: tuple
    allowed_ssh_targets: frozenset


def load_policy() -> Policy:
    try:
        raw = json.loads(POLICY_PATH.read_text(encoding="utf-8"))
        allowed_ips = frozenset(
            ipaddress.ip_address(str(value)) for value in raw.get("allowed_ips", [])
        )
        networks = tuple(
            ipaddress.ip_network(str(value)) for value in raw.get("documentation_networks", [])
        )
        ssh_targets = frozenset(str(value) for value in raw.get("allowed_ssh_targets", []))
    except Exception as exc:
        raise RuntimeError(f"invalid gate policy {POLICY_REL}: {exc}") from exc
    return Policy(allowed_ips, networks, ssh_targets)


@dataclass(frozen=True)
class Finding:
    path: Path
    line: int
    column: int
    kind: str
    value: str
    guidance: str


@dataclass(frozen=True)
class Rule:
    kind: str
    regex: re.Pattern[str]
    guidance: str


RULES: tuple[Rule, ...] = (
    Rule(
        "windows-absolute-path",
        WINDOWS_ABSOLUTE_PATH_RE,
        "use <workspace-root>, <user-home>, or an environment variable placeholder",
    ),
    Rule(
        "posix-user-path",
        POSIX_USER_PATH_RE,
        "use <workspace-root>, <user-home>, or $HOME-based placeholders",
    ),
    Rule(
        "server-filesystem-path",
        SERVER_PATH_RE,
        "use <server-root>, <deploy-root>, or a documented environment variable",
    ),
    Rule(
        "ssh-target",
        SSH_TARGET_RE,
        "use <user>@<host> or a service placeholder",
    ),
    Rule(
        "unc-server-path",
        UNC_PATH_RE,
        "use \\\\<server>\\<share> placeholders for UNC examples",
    ),
)


def run_git(args: Sequence[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", *args], cwd=ROOT, text=True, capture_output=True)


def git_paths(mode: str, rev_range: str) -> list[Path]:
    if mode == "worktree":
        proc = run_git(["ls-files", "--cached", "--others", "--exclude-standard"])
    elif mode == "tracked":
        proc = run_git(["ls-files"])
    elif mode == "staged":
        proc = run_git(["diff", "--cached", "--name-only", "--diff-filter=ACMR"])
    elif mode == "push":
        if not rev_range:
            raise ValueError("--range is required for --mode push")
        proc = run_git(["diff", "--name-only", "--diff-filter=ACMR", rev_range])
    else:
        raise ValueError(f"unsupported mode: {mode}")

    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or f"git command failed for mode {mode}")

    paths: list[Path] = []
    for raw in proc.stdout.splitlines():
        if not raw:
            continue
        if mode in ("push", "staged"):
            # Content comes from git objects, not the live worktree; membership
            # is decided by the diff, so no worktree existence check here.
            paths.append(Path(raw))
        else:
            path = ROOT / raw
            if path.is_file():
                paths.append(Path(raw))
    return sorted(set(paths))


def range_head(rev_range: str) -> str:
    head = rev_range.rsplit("..", 1)[-1].strip()
    return head or "HEAD"


def git_blob_bytes(rev: str, rel_path: Path) -> bytes | None:
    proc = subprocess.run(
        ["git", "show", f"{rev}:{rel_path.as_posix()}"],
        cwd=ROOT,
        capture_output=True,
    )
    if proc.returncode != 0:
        return None
    return proc.stdout


def load_item_bytes(mode: str, rev_range: str, rel_path: Path) -> bytes | None:
    if mode == "push":
        return git_blob_bytes(range_head(rev_range), rel_path)
    if mode == "staged":
        return git_blob_bytes("", rel_path)
    try:
        return (ROOT / rel_path).read_bytes()
    except OSError:
        return None


def is_probably_binary(data: bytes) -> bool:
    return b"\0" in data[:4096]


def binary_skip_reason(rel_path: Path) -> str | None:
    posix = rel_path.as_posix()
    for prefix in BINARY_SKIP_PREFIXES:
        if posix.startswith(prefix):
            return "binary input under " + prefix
    return None


def should_skip_ip(raw: str, policy: Policy) -> bool:
    try:
        ip = ipaddress.ip_address(raw)
    except ValueError:
        return True
    if ip in policy.allowed_ips or ip.is_loopback or ip.is_unspecified:
        return True
    return any(ip in net for net in policy.documentation_networks)


def line_column(line: str, start: int) -> int:
    return len(line[:start]) + 1


def scan_line(rel_path: Path, line_no: int, line: str, policy: Policy) -> Iterable[Finding]:
    if SUPPRESSION_TOKEN in line:
        return

    for rule in RULES:
        for match in rule.regex.finditer(line):
            value = match.group(0)
            if rule.kind == "ssh-target" and value in policy.allowed_ssh_targets:
                continue
            yield Finding(rel_path, line_no, line_column(line, match.start()), rule.kind, value, rule.guidance)

    for match in IPV4_RE.finditer(line):
        value = match.group(0)
        if should_skip_ip(value, policy):
            continue
        yield Finding(
            rel_path,
            line_no,
            line_column(line, match.start()),
            "ip-address",
            value,
            "use <server-ip>, <private-ip>, or a TEST-NET documentation address",
        )


def scan_item(rel_path: Path, data: bytes, policy: Policy) -> list[Finding]:
    text = data.decode("utf-8", errors="replace")
    findings: list[Finding] = []
    for line_no, line in enumerate(text.splitlines(), start=1):
        findings.extend(scan_line(rel_path, line_no, line, policy))
    return findings


def emit_findings(findings: Sequence[Finding]) -> None:
    for finding in findings:
        rel = finding.path.as_posix()
        print(
            "[local-info-leak] WARNING "
            f"{rel}:{finding.line}:{finding.column}: {finding.kind}: "
            f"{finding.value!r}; {finding.guidance}",
            file=sys.stderr,
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Scan repository-visible text files for developer-machine or server-specific "
            "paths, endpoints, and deployment hints that should be placeholders."
        )
    )
    parser.add_argument(
        "--mode",
        choices=("worktree", "tracked", "staged", "push"),
        default="worktree",
        help="File set to scan. worktree scans tracked plus unignored untracked files.",
    )
    parser.add_argument("--range", default="", help="Revision range for --mode push.")
    parser.add_argument(
        "--warning-only",
        action="store_true",
        help="Print warnings but exit 0. Delivery gates should not use this option.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        policy = load_policy()
        paths = git_paths(args.mode, args.range)
    except Exception as exc:
        print(f"[local-info-leak] failed: {exc}", file=sys.stderr)
        return 2

    findings: list[Finding] = []
    unreviewed: list[tuple[Path, str]] = []
    skipped_binary = 0
    scanned = 0
    for path in paths:
        if path.as_posix() == POLICY_REL:
            continue
        data = load_item_bytes(args.mode, args.range, path)
        if data is None:
            unreviewed.append((path, "content not readable"))
            continue
        if is_probably_binary(data):
            reason = binary_skip_reason(path)
            if reason is not None:
                skipped_binary += 1
            else:
                unreviewed.append((path, "binary content outside policy"))
            continue
        scanned += 1
        findings.extend(scan_item(path, data, policy))

    emit_findings(findings)
    for path, reason in unreviewed:
        print(f"[local-info-leak] UNREVIEWED {path.as_posix()}: {reason}", file=sys.stderr)

    if unreviewed:
        print(
            f"[local-info-leak] failed: {len(unreviewed)} file(s) could not be reviewed; resolve or narrow policy first.",
            file=sys.stderr,
        )
        return 2

    if findings:
        print(
            f"[local-info-leak] {len(findings)} warning(s); replace concrete local/server information with placeholders.",
            file=sys.stderr,
        )
        return 0 if args.warning_only else 1

    suffix = f", {skipped_binary} binary skipped by policy" if skipped_binary else ""
    print(f"[local-info-leak] ok ({scanned} file(s) scanned{suffix})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
