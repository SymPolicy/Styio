#!/usr/bin/env python3
"""
Compare two styio_core_bench JSON results and report regressions.

Usage:
  python3 scripts/benchmark-compare.py baseline.json current.json [--threshold 5.0]

Exit code is 0 when all benchmarks are within threshold.
Exit code is 1 when one or more benchmarks degrade beyond threshold.
"""

import json
import sys
import argparse
from pathlib import Path


def load_results(path: str) -> dict:
    with open(path, "r") as f:
        data = json.load(f)

    samples = {}
    for s in data.get("samples", []):
        key = f"{s['phase']}/{s['label']}"
        samples[key] = s["duration_ns"]
    return {
        "git_sha": data.get("git_sha", "unknown"),
        "build_type": data.get("build_type", "unknown"),
        "samples": samples,
    }


def compare(baseline: dict, current: dict, threshold_pct: float) -> int:
    baseline_samples = baseline["samples"]
    current_samples = current["samples"]

    all_keys = sorted(set(baseline_samples.keys()) | set(current_samples.keys()))

    print(f"Baseline: {baseline['git_sha']} ({baseline['build_type']})")
    print(f"Current:  {current['git_sha']} ({current['build_type']})")
    print(f"Threshold: {threshold_pct}%")
    print()
    print(f"{'Benchmark':<45} {'Baseline':>12} {'Current':>12} {'Delta %':>10} {'Status':>12}")
    print("-" * 93)

    regressions = 0
    improvements = 0
    missing = 0
    new_benchmarks = 0

    for key in all_keys:
        base_ns = baseline_samples.get(key)
        curr_ns = current_samples.get(key)

        if base_ns is None:
            print(f"{key:<45} {'N/A':>12} {_fmt_ns(curr_ns):>12} {'N/A':>10} {'NEW':>12}")
            new_benchmarks += 1
            continue

        if curr_ns is None:
            print(f"{key:<45} {_fmt_ns(base_ns):>12} {'N/A':>12} {'N/A':>10} {'MISSING':>12}")
            missing += 1
            continue

        delta_pct = ((curr_ns - base_ns) / base_ns) * 100.0

        if delta_pct > threshold_pct:
            status = "REGRESSION"
            regressions += 1
        elif delta_pct < -threshold_pct:
            status = "improved"
            improvements += 1
        else:
            status = "stable"

        print(
            f"{key:<45} {_fmt_ns(base_ns):>12} {_fmt_ns(curr_ns):>12} "
            f"{delta_pct:>+9.1f}% {status:>12}"
        )

    print()
    print(f"Summary: {regressions} regression(s), {improvements} improvement(s), "
          f"{new_benchmarks} new, {missing} missing")

    if regressions > 0:
        print(f"\nFAIL: {regressions} benchmark(s) degraded beyond {threshold_pct}% threshold.")
        return 1
    else:
        print(f"\nPASS: all benchmarks within {threshold_pct}% threshold.")
        return 0


def _fmt_ns(ns: int) -> str:
    if ns >= 1_000_000_000:
        return f"{ns / 1_000_000_000:.2f} s"
    elif ns >= 1_000_000:
        return f"{ns / 1_000_000:.2f} ms"
    elif ns >= 1_000:
        return f"{ns / 1_000:.2f} us"
    else:
        return f"{ns} ns"


def main():
    parser = argparse.ArgumentParser(
        description="Compare styio_core_bench JSON results"
    )
    parser.add_argument("baseline", help="Path to baseline JSON file")
    parser.add_argument("current", help="Path to current JSON file")
    parser.add_argument(
        "--threshold", type=float, default=5.0,
        help="Regression threshold in percent (default: 5.0)"
    )
    args = parser.parse_args()

    if not Path(args.baseline).exists():
        print(f"ERROR: baseline file not found: {args.baseline}")
        sys.exit(2)
    if not Path(args.current).exists():
        print(f"ERROR: current file not found: {args.current}")
        sys.exit(2)

    baseline = load_results(args.baseline)
    current = load_results(args.current)

    return compare(baseline, current, args.threshold)


if __name__ == "__main__":
    sys.exit(main())
