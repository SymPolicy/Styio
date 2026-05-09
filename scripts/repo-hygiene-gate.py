#!/usr/bin/env python3
from __future__ import annotations

import argparse
import fnmatch
import os
import subprocess
import sys
from pathlib import Path

DEFAULT_MAX_FILE_BYTES = 20 * 1024 * 1024
TEXT_EXTENSIONS = {
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".css",
    ".csv",
    ".def",
    ".h",
    ".hpp",
    ".html",
    ".js",
    ".json",
    ".jsonl",
    ".md",
    ".py",
    ".sh",
    ".svg",
    ".toml",
    ".ts",
    ".tsx",
    ".txt",
    ".xml",
    ".yaml",
    ".yml",
}
FORBIDDEN_GLOBS = [
    ".DS_Store",
    ".cursor/*",
    ".idea/*",
    ".kilo/*",
    ".vscode/*",
    "__pycache__/*",
    "*.a",
    "*.dSYM/*",
    "*.dll",
    "*.dylib",
    "*.exe",
    "*.gcda",
    "*.gcno",
    "*.log",
    "*.obj",
    "*.o",
    "*.profdata",
    "*.profraw",
    "*.so",
    "*.tmp",
    "CMakeCache.txt",
    "CMakeFiles/*",
    "Makefile",
    "Testing/*",
    "benchmark/reports/*",
    "build",
    "build/*",
    "build-*",
    "build-*/*",
    "cmake_install.cmake",
    "cmake_test_discovery_*.json",
    "compile_commands.json",
    "fuzz-artifacts/*",
    "output.ll",
    "parser-shadow-artifacts/*",
    "parser-shadow-artifacts-*/*",
    "reports/*",
    "sample.ll",
    "sanitizer-artifacts/*",
    "soak-artifacts/*",
    "docs/audit/defects/*",
    "styio",
    "styio_security_test",
    "styio_test",
]
ALWAYS_ALLOWED = {
    "benchmark/reports/README.md",
}
REQUIRED_GITIGNORE_PATTERNS = [
    ".DS_Store",
    ".cursor/",
    ".idea/",
    ".clangd/",
    ".cache/",
    "__pycache__/",
    ".pytest_cache/",
    ".mypy_cache/",
    ".ruff_cache/",
    ".venv/",
    "venv/",
    "node_modules/",
    "build/",
    "build-*/",
    "tmp/",
    "*.tmp",
    "*.log",
    "docs/audit/defects/",
    "!docs/**/build/",
    "!docs/**/build/**",
    "!docs/**/build-*/",
    "!docs/**/build-*/**",
    "!docs/**/tmp/",
    "!docs/**/tmp/**",
    "!docs/**/*.tmp",
    "!docs/**/*.log",
    "!tests/**/build/",
    "!tests/**/build/**",
    "!tests/**/build-*/",
    "!tests/**/build-*/**",
    "!tests/**/tmp/",
    "!tests/**/tmp/**",
    "!tests/**/*.tmp",
    "!tests/**/*.log",
]
REQUIRED_DOC_REFERENCES = {
    Path("docs/README.md"): [
        "scripts/docs-index.py",
        "scripts/docs-lifecycle.py",
        "scripts/docs-audit.py",
    ],
    Path("workflows/REPO-HYGIENE-COMMIT-STANDARD.md"): [
        "scripts/repo-hygiene-gate.py",
        "scripts/delivery-gate.sh",
    ],
}


def run_git(repo_root: Path, *args: str, check: bool = True) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo_root), *args],
        check=check,
        text=True,
        capture_output=True,
    )
    return result.stdout


def repo_root_from_cwd() -> Path:
    out = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        check=True,
        text=True,
        capture_output=True,
    )
    return Path(out.stdout.strip())


def load_allowlist(path: Path) -> set[str]:
    if not path.exists():
        return set()

    entries: set[str] = set()
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        entries.add(line)
    return entries


def is_allowlisted(path: str, allowlist: set[str]) -> bool:
    if path in ALWAYS_ALLOWED or path in allowlist:
        return True

    for entry in allowlist:
        if not entry.startswith("glob:"):
            continue
        pattern = entry[len("glob:") :]
        if fnmatch.fnmatch(path, pattern):
            return True

    return False


def staged_files(repo_root: Path) -> list[str]:
    out = run_git(
        repo_root,
        "diff",
        "--cached",
        "--name-only",
        "--diff-filter=ACMR",
    )
    return [line for line in out.splitlines() if line]


def worktree_files(repo_root: Path) -> list[str]:
    out = run_git(
        repo_root,
        "status",
        "--porcelain=v1",
        "--untracked-files=all",
    )
    paths: list[str] = []
    for line in out.splitlines():
        if not line:
            continue
        status = line[:2].strip()
        if status == "D":
            continue
        raw = line[3:]
        if " -> " in raw:
            raw = raw.split(" -> ", 1)[1]
        raw = raw.strip().strip('"')
        if raw:
            paths.append(raw)
    return sorted(set(paths))


def tracked_files(repo_root: Path) -> list[str]:
    out = run_git(repo_root, "ls-files")
    return [line for line in out.splitlines() if line]


def match_forbidden(path: str) -> str | None:
    for pattern in FORBIDDEN_GLOBS:
        if fnmatch.fnmatch(path, pattern):
            return pattern
    return None


def is_binary_file(path: Path) -> bool:
    data = path.read_bytes()[:8192]
    if not data:
        return False
    if b"\x00" in data:
        return True
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return True
    return False


def size_violations(repo_root: Path, files: list[str], max_bytes: int, allowlist: set[str]) -> list[str]:
    problems: list[str] = []
    for rel in files:
        if is_allowlisted(rel, allowlist):
            continue
        path = repo_root / rel
        if not path.is_file():
            continue
        size = path.stat().st_size
        if size > max_bytes:
            problems.append(
                f"{rel}: file size {size} bytes exceeds soft limit {max_bytes} bytes"
            )
    return problems


def binary_violations(repo_root: Path, files: list[str], allowlist: set[str]) -> list[str]:
    problems: list[str] = []
    for rel in files:
        if is_allowlisted(rel, allowlist):
            continue
        path = repo_root / rel
        if not path.is_file():
            continue
        if path.suffix.lower() in TEXT_EXTENSIONS:
            continue
        if is_binary_file(path):
            problems.append(
                f"{rel}: binary-looking file detected; add an explicit allowlist entry if intentional"
            )
    return problems


def path_violations(files: list[str], allowlist: set[str]) -> list[str]:
    problems: list[str] = []
    for rel in files:
        if is_allowlisted(rel, allowlist):
            continue
        pattern = match_forbidden(rel)
        if pattern is not None:
            problems.append(f"{rel}: matches forbidden pattern {pattern}")
    return problems


def history_violations(repo_root: Path, rev_range: str, max_bytes: int, allowlist: set[str]) -> list[str]:
    rev_list = subprocess.run(
        ["git", "-C", str(repo_root), "rev-list", "--objects", rev_range],
        check=True,
        text=True,
        capture_output=True,
    )
    if not rev_list.stdout.strip():
        return []

    batch = subprocess.run(
        [
            "git",
            "-C",
            str(repo_root),
            "cat-file",
            "--batch-check=%(objecttype) %(objectname) %(objectsize) %(rest)",
        ],
        input=rev_list.stdout,
        check=True,
        text=True,
        capture_output=True,
    )
    problems: list[str] = []
    for line in batch.stdout.splitlines():
        parts = line.split(" ", 3)
        if len(parts) != 4:
            continue
        object_type, _oid, object_size, path = parts
        if object_type != "blob":
            continue
        if not path:
            continue
        try:
            size = int(object_size)
        except ValueError:
            continue
        if is_allowlisted(path, allowlist):
            continue
        pattern = match_forbidden(path)
        if pattern is not None:
            problems.append(
                f"{path}: appears in pushed history range {rev_range} and matches forbidden pattern {pattern}"
            )
        if size > max_bytes:
            problems.append(
                f"{path}: blob size {size} bytes in pushed history range {rev_range} exceeds soft limit {max_bytes} bytes"
            )
    return sorted(set(problems))


def gitignore_pattern_violations(repo_root: Path) -> list[str]:
    gitignore = repo_root / ".gitignore"
    if not gitignore.exists():
        return [".gitignore is missing"]

    patterns = {
        line.strip()
        for line in gitignore.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    }
    problems: list[str] = []
    for required in REQUIRED_GITIGNORE_PATTERNS:
        if required not in patterns:
            problems.append(f".gitignore must include: {required}")
    return problems


def doc_reference_violations(repo_root: Path) -> list[str]:
    problems: list[str] = []
    for relative_path, needles in REQUIRED_DOC_REFERENCES.items():
        path = repo_root / relative_path
        if not path.exists():
            problems.append(f"required documentation file is missing: {relative_path.as_posix()}")
            continue
        text = path.read_text(encoding="utf-8")
        for needle in needles:
            if needle not in text:
                problems.append(f"{relative_path.as_posix()} must document {needle}")
    return problems


def upstream_range(repo_root: Path) -> str:
    probe = subprocess.run(
        ["git", "-C", str(repo_root), "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"],
        text=True,
        capture_output=True,
    )
    if probe.returncode == 0:
        return "@{u}..HEAD"
    return "HEAD"


def print_report(header: str, problems: list[str]) -> int:
    if not problems:
      print(f"[repo-hygiene] {header}: OK")
      return 0

    print(f"[repo-hygiene] {header}: FAILED", file=sys.stderr)
    for problem in problems:
        print(f"  - {problem}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Styio repository hygiene gate")
    parser.add_argument(
        "--mode",
        choices=("worktree", "staged", "tracked", "push"),
        default="staged",
        help="What to validate",
    )
    parser.add_argument(
        "--range",
        dest="rev_range",
        help="Explicit revision range for --mode push (defaults to @{u}..HEAD or HEAD)",
    )
    parser.add_argument(
        "--max-file-bytes",
        type=int,
        default=DEFAULT_MAX_FILE_BYTES,
        help="Soft file/blob size limit in bytes",
    )
    args = parser.parse_args()

    repo_root = repo_root_from_cwd()
    allowlist = load_allowlist(repo_root / "configs" / "repo-hygiene-allowlist.txt")

    if args.mode == "push":
        rev_range = args.rev_range or upstream_range(repo_root)
        problems = history_violations(repo_root, rev_range, args.max_file_bytes, allowlist)
        problems.extend(gitignore_pattern_violations(repo_root))
        problems.extend(doc_reference_violations(repo_root))
        problems = sorted(set(problems))
        return print_report(f"push range {rev_range}", problems)

    if args.mode == "worktree":
        files = worktree_files(repo_root)
    elif args.mode == "staged":
        files = staged_files(repo_root)
    else:
        files = tracked_files(repo_root)
    if not files:
        print(f"[repo-hygiene] {args.mode}: nothing to check")
        return 0

    problems = []
    problems.extend(path_violations(files, allowlist))
    problems.extend(size_violations(repo_root, files, args.max_file_bytes, allowlist))
    problems.extend(binary_violations(repo_root, files, allowlist))
    problems.extend(gitignore_pattern_violations(repo_root))
    problems.extend(doc_reference_violations(repo_root))
    problems = sorted(set(problems))
    return print_report(args.mode, problems)


if __name__ == "__main__":
    sys.exit(main())
