#!/usr/bin/env python3
"""Portable parser legacy entry audit used by CTest."""

from __future__ import annotations

from pathlib import Path
import re
import sys


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def search_sources(pattern: str, target: Path) -> list[str]:
    regex = re.compile(pattern)
    hits: list[str] = []
    if target.is_file():
        paths = [target]
    else:
        paths = [p for p in target.rglob("*") if p.is_file()]
    root = repo_root()
    for path in paths:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        rel = path.relative_to(root).as_posix()
        for line_no, line in enumerate(text.splitlines(), start=1):
            if regex.search(line):
                hits.append(f"{rel}:{line_no}:{line}")
    return hits


def report(title: str, hits: list[str]) -> None:
    print(f"[parser-legacy-audit] {title}", file=sys.stderr)
    for hit in hits:
        print(hit, file=sys.stderr)


def main() -> int:
    root = repo_root()
    violations = 0

    legacy_hits = [
        hit
        for hit in search_sources(r"parse_main_block_legacy\(", root / "src")
        if not hit.startswith("src/StyioParser/Parser.cpp:")
        and not hit.startswith("src/StyioParser/Parser.hpp:")
    ]
    if legacy_hits:
        report("unexpected direct parse_main_block_legacy(...) callsites outside parser core", legacy_hits)
        violations = 1

    testing_hits = search_sources(
        r"StyioParserEngine::Legacy|parse_main_block_legacy\(",
        root / "src" / "StyioTesting",
    )
    if testing_hits:
        report("src/StyioTesting must stay nightly-first; legacy routing is not allowed here", testing_hits)
        violations = 1

    main_hits = search_sources(r"parse_main_block_with_engine_latest\(", root / "src" / "main.cpp")
    if not main_hits:
        print(
            "[parser-legacy-audit] src/main.cpp no longer routes through parse_main_block_with_engine_latest(...)",
            file=sys.stderr,
        )
        violations = 1

    if violations:
        return 1
    print("[parser-legacy-audit] ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
