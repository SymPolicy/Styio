# Performance / Stability Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of benchmark routes, soak tests, performance reports, regression templates, and stability guardrails.

**Last updated:** 2026-04-30

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
7. `src/StyioProfiler/`

High-value docs:

1. [../design/performance-testing.md](../design/performance-testing.md)
2. [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md)

## Daily Workflow

1. Decide whether the question is compile-stage, micro hotspot, full-stack wall time, error-path, or soak stability.
2. Use structured outputs under `benchmark/reports/<run-id>/`; compare `results.json` or `benchmarks.csv`, not screenshots.
3. Keep benchmark workloads representative and tied to `benchmark/COVERAGE-MATRIX.md`.
4. Minimize soak failures before handing them to implementation owners.
5. Keep deep routes out of routine PR gates unless they protect an active high-risk change.
6. When native `@extern` performance changes, measure both first-run compile cost and cached repeated-run cost. Cache results are only comparable when `STYIO_NATIVE_CACHE_DIR`, compiler command, and source hash inputs are controlled.
7. For Styio-language frontend attribution, run `styio --profile-frontend --profile-out <report.json> --file <case.styio>` first. The report is `styio-profiler` JSON scoped to Styio phases such as source read, tokenize, parser context creation, parse, type inference, and Styio IR lowering, plus token histogram and parser-route counters.
8. Use LLVM XRay when benchmark deltas need C++ function-level attribution and `perf` is unavailable. Build an instrumented profile with `-fxray-instrument -fxray-instruction-threshold=1`, run with `XRAY_OPTIONS='patch_premain=true xray_mode=xray-basic xray_logfile_base=/tmp/styio-xray'`, then inspect with `llvm-xray account -instr_map=<instrumented-styio> -sort=sum -sortorder=dsc -top=30`. Treat XRay output as native profiler evidence, not Styio frontend attribution or release latency, because instrumentation inflates wall time.
9. Keep benchmark phase names aligned with the compiler middle-layer split: type inference maps to `StyioSemaContext`, and StyioIR lowering maps to `AstToStyioIRLowerer`.

## Change Classes

1. Small: benchmark label, report formatting, or smoke workload cleanup. Run quick route.
2. Medium: new benchmark dimension, soak case, or RSS guard. Update coverage matrix and run relevant labels.
3. High: changed threshold, nightly route, sanitizer/perf gate, or regression artifact workflow. Use checkpoint workflow and add ADR for durable gate policy.

## Required Gates

Quick route:

```bash
./benchmark/perf-route.sh --quick
ctest --test-dir build/default -L soak_smoke
```

Focused benchmark route:

```bash
./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20
```

Deep stability:

```bash
ctest --test-dir build/default -L soak_deep
./benchmark/soak-minimize.sh --help
```

`styio_soak_test` 若需要包含 LLVM 支持库头，必须通过共享的 `styio_apply_llvm_compile_settings(...)` helper 注入 LLVM include path，使其以 `-idirafter` 形式落在标准库头之后；不要直接给 benchmark 目标加 `SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS}`，否则 Debian + libstdc++ 会命中错误的 `cxxabi.h`。

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
