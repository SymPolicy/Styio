#!/usr/bin/env python3
from __future__ import annotations

import os
import sys
from pathlib import Path


def find_benchmark_root(styio_root: Path) -> Path:
    env_root = os.environ.get("STYIO_BENCHMARK_ROOT")
    if env_root:
        candidate = Path(env_root).expanduser().resolve()
        if (candidate / "tools" / "perf-report.py").is_file():
            return candidate
        raise SystemExit(f"STYIO_BENCHMARK_ROOT does not contain tools/perf-report.py: {candidate}")

    candidates = [
        styio_root.parent / "styio-benchmark",
        styio_root.parent.parent / "SymPolicy" / "styio-benchmark",
        Path.home() / "SymPolicy" / "styio-benchmark",
    ]
    for candidate in candidates:
        if (candidate / "tools" / "perf-report.py").is_file():
            return candidate.resolve()

    raise SystemExit("Unable to find styio-benchmark. Set STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark.")


def main() -> None:
    styio_root = Path(__file__).resolve().parents[1]
    target = find_benchmark_root(styio_root) / "tools" / "perf-report.py"
    os.execv(sys.executable, [sys.executable, str(target), *sys.argv[1:]])


if __name__ == "__main__":
    main()
