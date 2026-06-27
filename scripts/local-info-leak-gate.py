#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ipaddress
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

ROOT = Path(__file__).resolve().parents[1]
MAX_TEXT_BYTES = 5_000_000
SUPPRESSION_TOKEN = "local-info: allow"

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

DOCUMENTATION_NETWORKS = tuple(
    ipaddress.ip_network(net)
    for net in ("192.0.2.0/24", "198.51.100.0/24", "203.0.113.0/24")
)
ALLOWED_IPS = {
    ipaddress.ip_address("0.0.0.0"),
    ipaddress.ip_address("127.0.0.1"),
    ipaddress.ip_address("255.255.255.255"),
}
ALLOWED_SSH_TARGETS = {
    "git@github.com:",
}


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
        path = ROOT / raw
        if path.is_file():
            paths.append(Path(raw))
    return sorted(set(paths))


def is_probably_binary(data: bytes) -> bool:
    return b"\0" in data[:4096]


def read_text(rel_path: Path) -> str | None:
    path = ROOT / rel_path
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if len(data) > MAX_TEXT_BYTES or is_probably_binary(data):
        return None
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("utf-8", errors="replace")


def should_skip_ip(raw: str) -> bool:
    try:
        ip = ipaddress.ip_address(raw)
    except ValueError:
        return True
    if ip in ALLOWED_IPS or ip.is_loopback or ip.is_unspecified:
        return True
    return any(ip in net for net in DOCUMENTATION_NETWORKS)


def line_column(line: str, start: int) -> int:
    return len(line[:start]) + 1


def scan_line(rel_path: Path, line_no: int, line: str) -> Iterable[Finding]:
    if SUPPRESSION_TOKEN in line:
        return

    for rule in RULES:
        for match in rule.regex.finditer(line):
            value = match.group(0)
            if rule.kind == "ssh-target" and value in ALLOWED_SSH_TARGETS:
                continue
            yield Finding(rel_path, line_no, line_column(line, match.start()), rule.kind, value, rule.guidance)

    for match in IPV4_RE.finditer(line):
        value = match.group(0)
        if should_skip_ip(value):
            continue
        yield Finding(
            rel_path,
            line_no,
            line_column(line, match.start()),
            "ip-address",
            value,
            "use <server-ip>, <private-ip>, or a TEST-NET documentation address",
        )


def scan_path(rel_path: Path) -> list[Finding]:
    text = read_text(rel_path)
    if text is None:
        return []
    findings: list[Finding] = []
    for line_no, line in enumerate(text.splitlines(), start=1):
        findings.extend(scan_line(rel_path, line_no, line))
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
        paths = git_paths(args.mode, args.range)
    except Exception as exc:
        print(f"[local-info-leak] failed: {exc}", file=sys.stderr)
        return 2

    findings: list[Finding] = []
    for path in paths:
        findings.extend(scan_path(path))

    if findings:
        emit_findings(findings)
        print(
            f"[local-info-leak] {len(findings)} warning(s); replace concrete local/server information with placeholders.",
            file=sys.stderr,
        )
        return 0 if args.warning_only else 1

    print(f"[local-info-leak] ok ({len(paths)} file(s) scanned)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
