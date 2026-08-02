# Performance / Stability Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of benchmark routes, soak tests, performance reports, regression templates, and stability guardrails.

**Last updated:** 2026-08-02

## Mission

Own Styio's performance probe surface and long-run stability evidence. Canonical deep benchmark workloads, runners, reports, baselines, and cross-runtime comparisons live in `styio-benchmark`; the minimal `benchmark/core/` corpus stays in this repository so release-conformance can always emit local JSON timing evidence. This team protects the Styio-side probes, ABI, soak tiers, RSS guardrails, core benchmark smoke, and handoff to the external benchmark repository. It does not accept behavior changes without the implementation and Test Quality owners.

## Owned Surface

Primary paths:

1. `benchmark/CMakeLists.txt`
2. `benchmark/core/manifest.json`
3. `benchmark/core/run-core.py`
4. `styio-benchmark/styio-probes/styio_soak_test.cpp`
5. `styio-benchmark/styio-probes/styio_task_scheduler_perf_test.cpp`
6. `benchmark/perf-route.sh`
7. `benchmark/perf-report.py`
8. `benchmark/soak-minimize.sh`
9. `src/StyioProfiler/`
10. `scripts/benchmark-compare.py`

High-value docs:

1. [../design/performance-testing.md](../design/performance-testing.md)
2. [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md)
3. `styio-benchmark/README.md`
4. `styio-benchmark/docs/COVERAGE-MATRIX.md`

## Daily Workflow

1. Decide whether the question is compile-stage, micro hotspot, full-stack wall time, error-path, or soak stability.
2. Use structured outputs under `styio-benchmark/reports/<run-id>/`; compare `results.json` or `benchmarks.csv`, not screenshots. Use `scripts/benchmark-compare.py` for repository-local JSON regression comparisons, and add `--route-cache`, `--ir-alloc`, or `--scheduler` when the evidence needs parser route-cache counters, IR allocation counters, or task scheduler queue metadata.
3. Keep benchmark workloads representative and tied to `styio-benchmark/docs/COVERAGE-MATRIX.md`.
4. Keep `benchmark/core/manifest.json` tiny, deterministic, and backed by existing repository behavior coverage. It is release evidence for executable workloads and timing schema, not cross-runtime comparison or historical baseline evidence.
5. Minimize soak failures before handing them to implementation owners.
6. Keep deep routes out of routine PR gates unless they protect an active high-risk change.
7. When native `@extern` performance changes, measure both first-run compile cost and cached repeated-run cost. Cache results are only comparable when `STYIO_NATIVE_CACHE_DIR`, compiler command, and source hash inputs are controlled.
8. Task scheduler changes need a wall-clock concurrency proof. Keep `StyioTaskSchedulerPerf.SleepTasksRunConcurrently` green and record the sequential/concurrent ratio when changing `styio_task_*_spawn`, worker-count selection, blocking pull, or task handle release. The repository-local `styio_runtime_scheduler_test` covers the single bounded-wait queue through capacity-one producer wake-up, close/drain settlement, and multi-producer/multi-consumer exact-once interleavings. Repository-local `styio_core_bench` emits `scheduler/task_queue_mode` metadata with `task_scheduler_queue_kind` (`1` bounded wait), worker count, capacity, depth, pressure/wait, and lifecycle counters; `scripts/benchmark-compare.py --scheduler` reports scheduler metadata. These probes record repository-local scheduling facts; they are not evidence of a local speedup or sanitizer coverage.
9. For Styio-language attribution, run `styio --profile-frontend --profile-out <report.json> --file <case.styio>` first. The report is `styio-profiler` JSON scoped to source read, tokenize, parser context creation, parse, type inference, Styio IR lowering, runtime/JIT initialization, LLVM IR generation, and execution, plus token histogram, parser-route counters, and async scheduler counters/queue metadata.
10. For native executable run-only attribution, set `STYIO_NATIVE_PROFILE_OUT=<report.json>` while running a `styio build <file> -o <artifact>` output. The generated executable writes `styio-native-profiler` JSON with `runtime_init`, `execute`, and `runtime_check` phases; collect it during validation or a separate diagnostic run, not during measured repeats.
11. Use LLVM XRay when benchmark deltas need C++ function-level attribution and `perf` is unavailable. Build an instrumented profile with `-fxray-instrument -fxray-instruction-threshold=1`, run with `XRAY_OPTIONS='patch_premain=true xray_mode=xray-basic xray_logfile_base=/tmp/styio-xray'`, then inspect with `llvm-xray account -instr_map=<instrumented-styio> -sort=sum -sortorder=dsc -top=30`. Treat XRay output as native profiler evidence, not Styio frontend attribution or release latency, because instrumentation inflates wall time.
12. Keep benchmark phase names aligned with the compiler middle-layer split: type inference maps to `StyioSemaContext`, and StyioIR lowering maps to `AstToStyioIRLowerer`.
13. Async runtime comparisons must target the selected peer runtimes recorded by `styio-benchmark`. Do not replace them with generic thread pools when producing Styio task scheduler evidence.
14. Async runtime reports must include normalized per-workload performance columns. The per-workload normalization baseline is `1.00x`; lower scores show relative performance against that baseline. Use median samples, not single runs, when comparing no-op fanout.
15. Async runtime framework checks use pytest as the black-box contract runner over `styio-benchmark/async-runtime/run-async-bench.py`; keep runtime selection explicit with `--runtime` and promote only JSON/CSV/Markdown report outputs as evidence.
16. Native C++ comparisons must run from `styio-benchmark/native-cpp/` across the three standard routes: `full-cli`, `cached-jit`, and `runtime-only`. Use one generated input per workload and report both raw throughput and normalized relative performance. The per-route normalization baseline is `1.00x`; routes without a real implementation must be marked `unsupported`, not approximated by another route.
17. Soak workloads that exercise state-like behavior must use resource topology resource declarations, `expr -> @name` writes, and `@name[-1]` selectors. Retired state-resource spellings belong only in negative parser/security tests, not performance baselines.
18. Benchmark and stability helpers must stay native-Windows buildable when they are part of default targets. Prefer C++ standard library, `_popen`/`_pclose` guarded probes, CMake, or Python over POSIX-only shell commands so Windows CI can build `all` before running CTest without adding MSYS/Git Bash as a dependency.
19. Repository-local benchmark evidence tests may prove JSON serialization and compare-script handling for route-cache counters, IR-allocation counters, or scheduler queue metadata without presenting runtime improvement as established. Full speedup, allocation-reduction, or concurrency-safety statements still require stable benchmark JSON plus baseline/current comparison and the relevant sanitizer/profiler evidence.
20. Windows CTest benchmark entries that launch `styio.exe` must prepend the built runtime directory and resolved LLVM runtime directories through `ENVIRONMENT_MODIFICATION`. Do not rely on an interactive Developer PowerShell, WSL Bash path translation, or user-global PATH for `styio_core_benchmark_smoke`.
21. Measure persistent callable specialization reuse only with an explicit isolated `--callable-cache-dir`. Record both a cold run and a warm run with `--callable-cache-stats`; a valid warm comparison requires specialization `hits`, zero `writes` for already populated keys, and the path-free hashing/lookup/verification/materialization timing fields. Also retain a cache-disabled clean run because opt-in lookup and partition costs must never be presented as a default compiler regression. Corruption, retention, or concurrent-writer tests establish correctness, not speedup.
22. The OPT-C parser-core probes live in the repository-local `styio_core_bench` binary: `expr_flat_add_4096`, `expr_mixed_4096`, and `expr_right_power_64` must parse under phase `parse_expr` within `expression_token_visits <= 8 * token_count + 8`, zero expression-core scratch allocations, and bounded depth, with `alloc_count` recording `expression_ast_nodes`; the process fails on null parses, zero-fallback/zero-bridge violations, or counter-bound breaches. Median durations remain JSON evidence, not local thresholds. Gate: `ctest --test-dir build -R '^styio_core_benchmark_internal$|^styio_core_benchmark_json_output$' --output-on-failure`.

## Change Classes

1. Small: benchmark label, report formatting, core workload manifest cleanup, or smoke workload cleanup. Run quick route.
2. Medium: new benchmark dimension, soak case, or RSS guard. Update coverage matrix and run relevant labels.
3. High: changed threshold, nightly route, sanitizer/perf gate, or regression artifact workflow. Use checkpoint workflow and add ADR for durable gate policy.

## Required Gates

Quick route:

```bash
ctest --test-dir build/default -R '^styio_core_benchmark_smoke$' --output-on-failure
STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark ./benchmark/perf-route.sh --quick
ctest --test-dir build/default -L soak_smoke
```

The core benchmark smoke is always repository-local. `soak_smoke` requires the optional external `styio-benchmark/styio-probes` checkout to be resolved at configure time; checkpoint health skips that target when it is absent and records the skip explicitly.

Focused benchmark route:

```bash
STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark \
  ./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20

python3 scripts/benchmark-compare.py \
  auto benchmark/results/current.json \
  --baseline-dir benchmark/results \
  --route-cache \
  --ir-alloc \
  --scheduler \
  --threshold 5 \
  --markdown benchmark/results/current-route-cache-report.md
```

For a real before/after comparison, place a compatible `baseline.json` beside
`benchmark/results/current.json` or point `--baseline-dir` at the directory that
contains it. The Styio repository can validate the artifact workflow and report
counter deltas locally, but it cannot manufacture a `6e59b68` speedup proof on
its own because that commit predates the in-repo benchmark target.

Async runtime comparison:

```bash
cd /path/to/styio-benchmark
python3 -m pytest async-runtime/test_async_runtime_blackbox.py

async-runtime/run-async-bench.py \
  --styio-root /path/to/styio \
  --case baseline \
  --bootstrap-toolchains \
  --repeats 5 \
  --out-dir async-runtime/reports/<run-id>
```

The async comparison script defaults to `build/async-runtime-release`, configures that directory as CMake `Release` when needed, and rejects non-Release Styio build caches for cross-runtime performance reports.

Native C++ comparison:

```bash
cd /path/to/styio-benchmark
native-cpp/run-native-cpp-bench.py \
  --styio-root /path/to/styio \
  --routes all \
  --line-count 100000 \
  --line-bytes 48 \
  --repeats 5 \
  --out-dir reports/native-cpp-stdin-echo
```

Use this route when changing standard stream lowering, resource pipe codegen, CLI execution, output helper behavior, JIT caching, or runtime helper behavior. `full-cli` intentionally includes Styio frontend and JIT cost. `cached-jit` remains `unsupported` until Styio exposes a reusable compiled/JIT artifact execution contract. `runtime-only` currently compares native C++ against a C++ harness that calls Styio runtime helpers directly, so it isolates helper cost without pretending to be generated Styio code.

Deep stability:

```bash
ctest --test-dir build/default -L soak_deep
STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark ./benchmark/soak-minimize.sh --help
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
