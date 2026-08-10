#!/usr/bin/env python3
"""Portable parser correctness gate for shadow-parser feature suites."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def fail(message: str, code: int = 1) -> int:
    print(f"[shadow-gate] {message}", file=sys.stderr)
    return code


def is_executable_file(path: Path) -> bool:
    if not path.is_file():
        return False
    if os.name == "nt":
        return True
    return os.access(path, os.X_OK)


def load_expected_nonzero(suite_dir: Path) -> set[str]:
    path = suite_dir / "shadow-expected-nonzero.txt"
    if not path.is_file():
        return set()
    out: set[str] = set()
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            out.add(line)
    return out


def count_jsonl_status(artifact_root: Path, status: str) -> int:
    needle = f'"status":"{status}"'
    count = 0
    for path in artifact_root.rglob("*.jsonl"):
        text = path.read_text(encoding="utf-8", errors="replace")
        count += text.count(needle)
    return count


def count_jsonl_regex(artifact_root: Path, pattern: re.Pattern[str]) -> int:
    count = 0
    for path in artifact_root.rglob("*.jsonl"):
        text = path.read_text(encoding="utf-8", errors="replace")
        if pattern.search(text):
            count += 1
    return count


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-zero-fallback", action="store_true")
    parser.add_argument("--require-zero-internal-bridges", action="store_true")
    parser.add_argument("styio_bin", type=Path)
    parser.add_argument("suite_dir", type=Path)
    parser.add_argument("artifact_root", type=Path)
    args = parser.parse_args(argv)

    if not is_executable_file(args.styio_bin):
        return fail(f"compiler is not executable: {args.styio_bin}", 2)
    if not args.suite_dir.is_dir():
        return fail(f"suite dir not found: {args.suite_dir}", 2)

    args.artifact_root.mkdir(parents=True, exist_ok=True)
    expected_nonzero = load_expected_nonzero(args.suite_dir)

    total = 0
    failed = 0
    cases = 0
    allowed_nonzero_runs = 0
    expected_nonzero_mismatches = 0

    for input_path in sorted(args.suite_dir.glob("t*.styio")):
        if not input_path.is_file():
            continue
        cases += 1
        base_name = input_path.stem
        expect_nonzero_case = base_name in expected_nonzero
        for engine in ("legacy", "nightly"):
            total += 1
            run_dir = args.artifact_root / f"{base_name}-{engine}"
            run_dir.mkdir(parents=True, exist_ok=True)
            stderr_path = run_dir / "stderr.log"
            with stderr_path.open("wb") as stderr:
                result = subprocess.run(
                    [
                        str(args.styio_bin),
                        f"--parser-engine={engine}",
                        "--parser-shadow-compare",
                        "--parser-shadow-artifact-dir",
                        str(run_dir),
                        "--file",
                        str(input_path),
                    ],
                    stdout=subprocess.DEVNULL,
                    stderr=stderr,
                    check=False,
                )
            if result.returncode != 0:
                if expect_nonzero_case:
                    allowed_nonzero_runs += 1
                else:
                    print(
                        f"[shadow-gate] failed: case={base_name} engine={engine}",
                        file=sys.stderr,
                    )
                    failed += 1
            elif expect_nonzero_case:
                print(
                    f"[shadow-gate] expected nonzero exit but got success: case={base_name} engine={engine}",
                    file=sys.stderr,
                )
                expected_nonzero_mismatches += 1

    if cases == 0:
        return fail(f"no t*.styio case found under {args.suite_dir}", 2)

    jsonl_count = sum(1 for _ in args.artifact_root.rglob("*.jsonl"))
    match_count = count_jsonl_status(args.artifact_root, "match")
    mismatch_count = count_jsonl_status(args.artifact_root, "mismatch")
    shadow_error_count = count_jsonl_status(args.artifact_root, "shadow_error")
    nonzero_fallback_count = (
        count_jsonl_regex(args.artifact_root, re.compile(r"legacy_fallback_statements=[1-9]"))
        if args.require_zero_fallback
        else 0
    )
    nonzero_internal_bridge_count = (
        count_jsonl_regex(args.artifact_root, re.compile(r"nightly_internal_legacy_bridges=[1-9]"))
        if args.require_zero_internal_bridges
        else 0
    )

    passed = (
        failed == 0
        and expected_nonzero_mismatches == 0
        and mismatch_count == 0
        and shadow_error_count == 0
        and jsonl_count >= total
        and nonzero_fallback_count == 0
        and nonzero_internal_bridge_count == 0
    )
    summary = {
        "cases": cases,
        "runs": total,
        "artifacts": jsonl_count,
        "expected_nonzero_cases": len(expected_nonzero),
        "allowed_nonzero_runs": allowed_nonzero_runs,
        "expected_nonzero_mismatches": expected_nonzero_mismatches,
        "match": match_count,
        "mismatch": mismatch_count,
        "shadow_error": shadow_error_count,
        "nonzero_fallback_records": nonzero_fallback_count,
        "nonzero_internal_bridge_records": nonzero_internal_bridge_count,
        "failed_runs": failed,
        "passed": passed,
    }
    (args.artifact_root / "summary.json").write_text(
        json.dumps(summary, indent=2) + "\n",
        encoding="utf-8",
    )

    print(
        "[shadow-gate] "
        f"cases={cases} runs={total} artifacts={jsonl_count} "
        f"expected_nonzero_cases={len(expected_nonzero)} "
        f"allowed_nonzero_runs={allowed_nonzero_runs} "
        f"expected_nonzero_mismatches={expected_nonzero_mismatches} "
        f"match={match_count} mismatch={mismatch_count} "
        f"shadow_error={shadow_error_count} "
        f"nonzero_fallback={nonzero_fallback_count} "
        f"nonzero_internal_bridges={nonzero_internal_bridge_count}"
    )

    if jsonl_count < total:
        return fail(f"missing artifact records: expected at least {total}, got {jsonl_count}")
    if failed or expected_nonzero_mismatches or mismatch_count or shadow_error_count:
        return fail("gate failed")
    if args.require_zero_fallback and nonzero_fallback_count:
        return fail("zero-fallback gate failed")
    if args.require_zero_internal_bridges and nonzero_internal_bridge_count:
        return fail("zero-internal-bridges gate failed")

    print("[shadow-gate] gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
