#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ENTRY_FILE = Path("src/main.cpp")
MAX_LINES = 7584


def line_count(path: Path) -> int:
    return len(path.read_text(encoding="utf-8").splitlines())


def main() -> int:
    path = ROOT / ENTRY_FILE
    if not path.is_file():
        print(f"[monolith-line-ratchet] missing entry file: {ENTRY_FILE}", file=sys.stderr)
        return 1
    actual = line_count(path)
    if actual > MAX_LINES:
        print(
            f"[monolith-line-ratchet] FAILED: {ENTRY_FILE} has {actual} lines; ceiling is {MAX_LINES}",
            file=sys.stderr,
        )
        return 1
    print(f"[monolith-line-ratchet] OK: {ENTRY_FILE} has {actual}/{MAX_LINES} lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
