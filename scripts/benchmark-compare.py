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

ROUTE_CACHE_FIELDS = (
    ("scan", "route_cache_scan_count"),
    ("miss", "route_cache_miss_count"),
    ("hit", "route_cache_hit_count"),
    ("disabled", "route_cache_disabled_count"),
)


def load_results(path: str) -> dict:
    with open(path, "r") as f:
        data = json.load(f)

    # Support both v1 (samples) and v2 (benchmarks) schema
    bench_list = data.get("benchmarks", data.get("samples", []))
    samples = {}
    for s in bench_list:
        # v2: "name" = "phase/label"; v1: separate "phase"/"label"
        if "name" in s:
            key = s["name"]
        else:
            key = f"{s['phase']}/{s['label']}"
        is_route_cache = key.startswith("route_cache/")
        # Use median_ns if available, else duration_ns
        ns = s.get("median_ns", s.get("duration_ns", 0))
        samples[key] = {
            "median_ns": ns,
            "mean_ns": s.get("mean_ns", ns),
            "p95_ns": s.get("p95_ns", ns),
            "min_ns": s.get("min_ns", ns),
            "max_ns": s.get("max_ns", ns),
            "iterations": s.get("iterations", 0),
        }
        for _, source_key in ROUTE_CACHE_FIELDS:
            if source_key in s:
                samples[key][source_key] = s[source_key]
            elif is_route_cache:
                samples[key][source_key] = 0
    return {
        "git_sha": data.get("commit", data.get("git_sha", "unknown")),
        "build_type": data.get("build_type", "unknown"),
        "samples": samples,
    }


def compare(baseline: dict, current: dict, threshold_pct: float,
            markdown_path: str = None, allow_missing_new: bool = False,
            require_improvement: dict = None,
            show_route_cache: bool = False) -> int:
    baseline_samples = baseline["samples"]
    current_samples = current["samples"]

    all_keys = sorted(set(baseline_samples.keys()) | set(current_samples.keys()))

    lines = []
    md_lines = []

    def emit(s: str):
        lines.append(s)

    def md(s: str):
        md_lines.append(s)

    emit(f"Baseline: {baseline['git_sha']} ({baseline['build_type']})")
    emit(f"Current:  {current['git_sha']} ({current['build_type']})")
    emit(f"Threshold: {threshold_pct}%")
    emit("")
    emit(f"{'Benchmark':<45} {'Baseline':>12} {'Current':>12} {'Delta %':>10} {'Status':>12}")
    emit("-" * 93)

    md("| Benchmark | Baseline | Current | Delta % | Status |")
    md("|-----------|----------|---------|---------|--------|")

    regressions = 0
    improvements = 0
    missing = 0
    new_benchmarks = 0

    for key in all_keys:
        base = baseline_samples.get(key)
        curr = current_samples.get(key)

        if base is None:
            msg = f"{key:<45} {'N/A':>12} {_fmt_ns(curr['median_ns']):>12} {'N/A':>10} {'NEW':>12}"
            emit(msg)
            md(f"| {key} | N/A | {_fmt_ns(curr['median_ns'])} | N/A | NEW |")
            new_benchmarks += 1
            if not allow_missing_new:
                missing += 1
            continue

        if curr is None:
            msg = f"{key:<45} {_fmt_ns(base['median_ns']):>12} {'N/A':>12} {'N/A':>10} {'MISSING':>12}"
            emit(msg)
            md(f"| {key} | {_fmt_ns(base['median_ns'])} | N/A | N/A | MISSING |")
            missing += 1
            continue

        base_ns = base["median_ns"]
        curr_ns = curr["median_ns"]

        if base_ns == 0:
            delta_pct = 0.0
        else:
            delta_pct = ((curr_ns - base_ns) / base_ns) * 100.0

        if delta_pct > threshold_pct:
            status = "REGRESSION"
            regressions += 1
        elif delta_pct < -threshold_pct:
            status = "improved"
            improvements += 1
        else:
            status = "stable"

        emit(
            f"{key:<45} {_fmt_ns(base_ns):>12} {_fmt_ns(curr_ns):>12} "
            f"{delta_pct:>+9.1f}% {status:>12}"
        )
        md(f"| {key} | {_fmt_ns(base_ns)} | {_fmt_ns(curr_ns)} | {delta_pct:+.1f}% | {status} |")

    emit("")
    emit(f"Summary: {regressions} regression(s), {improvements} improvement(s), "
         f"{new_benchmarks} new, {missing} missing")
    md("")
    md(f"**Summary:** {regressions} regression(s), {improvements} improvement(s), "
       f"{new_benchmarks} new, {missing} missing")

    if show_route_cache:
        emit("")
        emit("Route cache counters:")
        emit(f"{'Benchmark':<45} {'Metric':<10} {'Baseline':>12} {'Current':>12} {'Delta':>12}")
        emit("-" * 93)
        md_lines.append("| Benchmark | Metric | Baseline | Current | Delta |")
        md_lines.append("|----------|--------|----------|---------|-------|")
        for key in all_keys:
            base_sample = baseline_samples.get(key)
            curr_sample = current_samples.get(key)
            if not (_has_route_cache_fields(base_sample) or _has_route_cache_fields(curr_sample)):
                continue
            for metric_name, source_key in ROUTE_CACHE_FIELDS:
                if base_sample is None or source_key not in base_sample:
                    base_val = None
                    base_display = "N/A"
                else:
                    base_val = base_sample[source_key]
                    base_display = str(base_val)

                if curr_sample is None or source_key not in curr_sample:
                    curr_val = None
                    curr_display = "N/A"
                else:
                    curr_val = curr_sample[source_key]
                    curr_display = str(curr_val)

                if base_val is None or curr_val is None:
                    delta_display = "N/A"
                else:
                    delta = curr_val - base_val
                    delta_display = f"{delta:+d}"

                emit(
                    f"{key:<45} {metric_name:<10} "
                    f"{base_display:>12} {curr_display:>12} {delta_display:>12}"
                )
                md_lines.append(
                    f"| {key} | {metric_name} | {base_display} | {curr_display} | {delta_display} |"
                )

    # Check required improvements
    req_failures = []
    if require_improvement:
        for bench_name, required_pct in require_improvement.items():
            if bench_name in baseline_samples and bench_name in current_samples:
                base_ns = baseline_samples[bench_name]["median_ns"]
                curr_ns = current_samples[bench_name]["median_ns"]
                if base_ns > 0:
                    actual_pct = -((base_ns - curr_ns) / base_ns) * 100.0
                    if actual_pct < required_pct:
                        req_failures.append(
                            f"{bench_name}: required {required_pct}% improvement, "
                            f"got {actual_pct:+.1f}%"
                        )
            else:
                req_failures.append(f"{bench_name}: missing from baseline or current")

    # Write markdown if requested
    if markdown_path:
        with open(markdown_path, "w") as f:
            f.write("\n".join(md_lines) + "\n")

    # Print output
    print("\n".join(lines))

    # Print requirement failures
    if req_failures:
        print(f"\nFAIL: {len(req_failures)} required improvement(s) not met:")
        for f in req_failures:
            print(f"  - {f}")

    if regressions > 0:
        print(f"\nFAIL: {regressions} benchmark(s) degraded beyond {threshold_pct}% threshold.")
        return 1
    if req_failures:
        return 1
    print(f"\nPASS: all benchmarks within {threshold_pct}% threshold.")
    return 0


def _has_route_cache_fields(sample) -> bool:
    if sample is None:
        return False
    return any(field in sample for _, field in ROUTE_CACHE_FIELDS)


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
    parser.add_argument(
        "--markdown", type=str, default=None,
        help="Write markdown summary to this path"
    )
    parser.add_argument(
        "--allow-missing-new", action="store_true", default=False,
        help="Allow new benchmarks in current that are missing from baseline"
    )
    parser.add_argument(
        "--require-improvement", type=str, default=None,
        help="Comma-separated bench:min_pct entries that must improve, e.g. 'parse/many_stmts_1k:10'"
    )
    parser.add_argument(
        "--route-cache", action="store_true", default=False,
        help="Include route cache counter summary in the report."
    )
    args = parser.parse_args()

    if not Path(args.baseline).exists():
        print(f"ERROR: baseline file not found: {args.baseline}")
        sys.exit(2)
    if not Path(args.current).exists():
        print(f"ERROR: current file not found: {args.current}")
        sys.exit(2)

    # Parse required improvements
    req_improve = {}
    if args.require_improvement:
        for item in args.require_improvement.split(","):
            item = item.strip()
            if ":" in item:
                name, pct = item.rsplit(":", 1)
                req_improve[name.strip()] = float(pct)

    baseline = load_results(args.baseline)
    current = load_results(args.current)

    return compare(baseline, current, args.threshold,
                   markdown_path=args.markdown,
                   allow_missing_new=args.allow_missing_new,
                   require_improvement=req_improve if req_improve else None,
                   show_route_cache=args.route_cache)


if __name__ == "__main__":
    sys.exit(main())
