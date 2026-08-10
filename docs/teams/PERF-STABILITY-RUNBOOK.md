# Performance / Stability Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of benchmark routes, soak tests, performance reports, regression templates, and stability guardrails.

**Last updated:** 2026-08-10

## Mission

Own the compiler-side profiler and the explicit integration seam used by `styio-benchmark`. All performance workloads, probe implementations, runners, reports, baselines, and cross-runtime comparisons live in `styio-benchmark`; this repository retains compiler correctness tests and `benchmark/CMakeLists.txt` only. It does not accept behavior changes without the implementation and Test Quality owners.

## Owned Surface

Primary paths:

1. `benchmark/CMakeLists.txt`
2. `src/StyioProfiler/`
3. `styio-benchmark/workloads/`
4. `styio-benchmark/styio-probes/`
5. `styio-benchmark/tools/`
6. `styio-benchmark/reports/`

High-value docs:

1. [../design/performance-testing.md](../design/performance-testing.md)
2. [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md)
3. `styio-benchmark/README.md`
4. `styio-benchmark/docs/COVERAGE-MATRIX.md`

## Daily Workflow

1. Decide whether the question is compile-stage, micro hotspot, full-stack wall time, error-path, or soak stability.
2. Use structured outputs under `styio-benchmark/reports/<run-id>/`; compare `results.json` or `benchmarks.csv`, not screenshots. Use `styio-benchmark/tools/core-benchmark-compare.py` for core JSON comparisons, including route-cache, IR-allocation, and scheduler counters.
3. Keep benchmark workloads representative and tied to `styio-benchmark/docs/COVERAGE-MATRIX.md`.
4. Keep `styio-benchmark/workloads/core/manifest.json` tiny, deterministic, and self-contained in the benchmark repository.
5. Minimize soak failures before handing them to implementation owners.
6. Keep deep routes out of routine PR gates unless they protect an active high-risk change.
7. When native `@extern` performance changes, measure both first-run compile cost and cached repeated-run cost. Cache results are only comparable when `STYIO_NATIVE_CACHE_DIR`, compiler command, and source hash inputs are controlled.
8. Task scheduler changes need a wall-clock concurrency proof. Keep `StyioTaskSchedulerPerf.SleepTasksRunConcurrently` green and record the sequential/concurrent ratio when changing `styio_task_*_spawn`, worker-count selection, blocking pull, or task handle release. The repository-local `styio_runtime_scheduler_test` covers the single bounded-wait queue through capacity-one producer wake-up, close/drain settlement, and multi-producer/multi-consumer exact-once interleavings. External `styio_core_bench` emits scheduler metadata, and `styio-benchmark/tools/core-benchmark-compare.py --scheduler` reports it.
9. For Styio-language attribution, run `styio --profile-frontend --profile-out <report.json> --file <case.styio>` first. The report is `styio-profiler` JSON scoped to source read, tokenize, parser context creation, parse, type inference, Styio IR lowering, runtime/JIT initialization, LLVM IR generation, and execution, plus token histogram, parser-route counters, and async scheduler counters/queue metadata.
10. For native executable run-only attribution, set `STYIO_NATIVE_PROFILE_OUT=<report.json>` while running a `styio build <file> -o <artifact>` output. The generated executable writes `styio-native-profiler` JSON with `runtime_init`, `execute`, and `runtime_check` phases; collect it during validation or a separate diagnostic run, not during measured repeats.
11. Use LLVM XRay when benchmark deltas need C++ function-level attribution and `perf` is unavailable. Build an instrumented profile with `-fxray-instrument -fxray-instruction-threshold=1`, run with `XRAY_OPTIONS='patch_premain=true xray_mode=xray-basic xray_logfile_base=/tmp/styio-xray'`, then inspect with `llvm-xray account -instr_map=<instrumented-styio> -sort=sum -sortorder=dsc -top=30`. Treat XRay output as native profiler evidence, not Styio frontend attribution or release latency, because instrumentation inflates wall time.
12. Keep benchmark phase names aligned with the compiler middle-layer split: type inference maps to `StyioSemaContext`, and StyioIR lowering maps to `AstToStyioIRLowerer`.
13. Async runtime comparisons must target the selected peer runtimes recorded by `styio-benchmark`. Do not replace them with generic thread pools when producing Styio task scheduler evidence.
14. Async runtime reports must include normalized per-workload performance columns. The per-workload normalization baseline is `1.00x`; lower scores show relative performance against that baseline. Use median samples, not single runs, when comparing no-op fanout.
15. Async runtime framework checks use pytest as the black-box contract runner over `styio-benchmark/async-runtime/run-async-bench.py`; keep runtime selection explicit with `--runtime` and promote only JSON/CSV/Markdown report outputs as evidence.
16. Standards parity comparisons run from `styio-benchmark/tools/standard_parity_gate.py` against `workloads/parity-v2/contract.json`. The closed route set is exactly `compile-and-run`, `native-build`, and `native-run`; each route has a distinct timed boundary and artifact policy. Use the same deterministic input, algorithm, static work unit, numeric order, and one-thread contract for Styio and C++20 `-O3 -DNDEBUG -fno-lto`. Capability-blocked cases remain outside aggregates; do not approximate them with another route or implementation. Compiler-owned runtime/user-object cache hits and phase profiles are diagnostic only and stay outside timed parity samples.
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
/path/to/styio-benchmark/tools/perf-route.sh --styio-root "$PWD" --quick
ctest --test-dir build/default -L soak_smoke
```

Core and soak benchmark tests are registered only when CMake receives an explicit `STYIO_BENCHMARK_ROOT`; standalone compiler builds discover nothing implicitly.

Focused benchmark route:

```bash
/path/to/styio-benchmark/tools/perf-route.sh \
  --styio-root "$PWD" --phase-iters 5000 --micro-iters 5000 --execute-iters 20

python3 /path/to/styio-benchmark/tools/core-benchmark-compare.py \
  auto /path/to/styio-benchmark/reports/core/current.json \
  --baseline-dir /path/to/styio-benchmark/reports/core \
  --route-cache \
  --ir-alloc \
  --scheduler \
  --threshold 5 \
  --markdown /path/to/styio-benchmark/reports/core/current-route-cache-report.md
```

For a real before/after comparison, place a compatible `baseline.json` beside
`styio-benchmark/reports/core/current.json` or point `--baseline-dir` at the directory that
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

Standards parity:

```bash
cd /path/to/styio-benchmark
python3 tools/standard_parity_gate.py catalog-check \
  --contract workloads/parity-v2/contract.json
python3 tools/standard_parity_gate.py run \
  --contract workloads/parity-v2/contract.json \
  --family clbg-n-body --scale smoke \
  --styio-root /path/to/styio-nightly \
  --build-dir /path/to/styio-nightly/build \
  --out-dir reports/standard-parity/shards/clbg-n-body \
  --warmups 3 --repetitions 11
python3 tools/standard_parity_gate.py verify \
  --contract workloads/parity-v2/contract.json \
  --report reports/standard-parity/shards/clbg-n-body/results.json \
  --privacy strict
```

Use the family shards when changing standard stream lowering, generated code,
runtime helpers, parser/sema/lowering, or pipeline orchestration. The runner
validates output before timing, alternates paired order, retains every sample,
and measures peak memory with an isolated replay rather than an observer in the
timed region. Smoke/development reports are tuning evidence; only a complete
reference-scale merge can support the strict 1.10 per-cell and 1.05 equal-weight
geometric-mean gates.

Deep stability:

```bash
ctest --test-dir build/default -L soak_deep
/path/to/styio-benchmark/tools/soak-minimize.sh --styio-root "$PWD" --help
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
