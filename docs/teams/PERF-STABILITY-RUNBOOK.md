# Performance / Stability Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of benchmark routes, soak tests, performance reports, regression templates, and stability guardrails.

**Last updated:** 2026-04-15

## Mission

Own performance and long-run stability evidence. This team protects benchmark coverage, soak tiers, RSS guardrails, error-path cost tracking, report comparability, and minimized regression artifacts. It does not accept behavior changes without the implementation and Test Quality owners.

## Owned Surface

Primary paths:

1. `benchmark/`
2. `benchmark/styio_soak_test.cpp`
3. `benchmark/perf-route.sh`
4. `benchmark/perf-report.py`
5. `benchmark/COVERAGE-MATRIX.md`
6. `benchmark/REGRESSION-TEMPLATE.md`

High-value docs:

1. [../design/performance-testing.md](../design/performance-testing.md)
2. [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md)

## Daily Workflow

1. Decide whether the question is compile-stage, micro hotspot, full-stack wall time, error-path, or soak stability.
2. Use structured outputs under `benchmark/reports/<run-id>/`; compare `results.json` or `benchmarks.csv`, not screenshots.
3. Keep benchmark workloads representative and tied to `benchmark/COVERAGE-MATRIX.md`.
4. Minimize soak failures before handing them to implementation owners.
5. Keep deep routes out of routine PR gates unless they protect an active high-risk change.

## Change Classes

1. Small: benchmark label, report formatting, or smoke workload cleanup. Run quick route.
2. Medium: new benchmark dimension, soak case, or RSS guard. Update coverage matrix and run relevant labels.
3. High: changed threshold, nightly route, sanitizer/perf gate, or regression artifact workflow. Use checkpoint workflow and add ADR for durable gate policy.

## Required Gates

Quick route:

```bash
./benchmark/perf-route.sh --quick
ctest --test-dir build -L soak_smoke
```

Focused benchmark route:

```bash
./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20
```

Deep stability:

```bash
ctest --test-dir build -L soak_deep
./benchmark/soak-minimize.sh --help
```

## Cross-Team Dependencies

1. Codegen / Runtime must review runtime loop, allocation, handle, and LLVM hotspot findings.
2. Frontend must review lexer/parser benchmark regressions.
3. Sema / IR must review type/lower/repr benchmark regressions.
4. Test Quality must review any benchmark promoted into a required gate.
5. Docs / Ecosystem must review benchmark documentation and report lifecycle changes.

## Handoff / Recovery

Record unfinished perf/stability work with:

1. Report directory and label.
2. Baseline and candidate command lines.
3. Metric that regressed and acceptable threshold.
4. Minimized workload or soak reproduction command.
5. Implementation team expected to investigate.
