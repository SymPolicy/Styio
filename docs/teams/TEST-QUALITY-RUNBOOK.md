# Test Quality Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of feature tests, golden files, five-layer pipeline cases, security tests, fuzz smoke, parser shadow gates, and test documentation.

**Last updated:** 2026-05-13

## Mission

Own the evidence that Styio behavior is accepted, reproducible, and recoverable. This team protects CTest registration, fixture layout, golden oracles, C++ reference equivalence cases, fuzz/security coverage, and test catalog accuracy. It does not decide language semantics without the design SSOT.

## Owned Surface

Primary paths:

1. `tests/`
2. `tests/CMakeLists.txt`
3. `tests/fuzz/`
4. `tests/algorithms/`
5. `tests/security/`
6. `src/StyioTesting/`
7. [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md)
8. [../assets/workflow/FIVE-LAYER-PIPELINE.md](../assets/workflow/FIVE-LAYER-PIPELINE.md)
9. [../assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md](../assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md)

## Daily Workflow

1. Identify the behavior owner before adding an oracle.
2. Choose the smallest useful test layer: feature stdout, semantic failure, five-layer, C++ unit, security, fuzz, shadow gate, or soak.
3. Register every new automated test in CMake.
4. Update [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) when adding or changing acceptance tests.
5. Keep generated or temporary outputs out of the repository unless the test framework explicitly treats them as goldens.
6. Treat compile-plan negative-path coverage and machine-readable diagnostics as contract evidence, not optional smoke coverage.
7. When compile-plan artifacts grow, add assertions for receipt fields and auxiliary artifacts such as `runtime-events.jsonl`, not just exit codes.
8. Keep five-layer Layer 4 LLVM goldens semantic, not implementation-bound: when stdout lowering moves between legacy `printf/puts` and runtime helpers such as `styio_stdout_write_cstr`, or when LLVM stops printing unused `declare` lines and renumbers transient `%<n>` temporaries, update the pipeline canonicalization before touching large golden sets.
9. Treat workflow scheduler tests as gate-level regression coverage; changes to scheduler profiles, phase ordering, or registry validation must update `tests/workflow_scheduler_test.py`.
10. Treat `StyioTaskSchedulerPerf.SleepTasksRunConcurrently` as the task_resources task_runtime performance sentinel. It must compare against an in-process sequential baseline rather than a fixed absolute timeout so CI variance does not hide loss of concurrency.
11. When compiler handoff contracts grow, add or update regression coverage for both `--machine-info=json` and `--source-build-info=json` so `spio`-facing metadata cannot drift silently.
12. When the compiler-side source-build helper changes, keep a lightweight regression on `scripts/source-build-minimal.sh --help` or an equivalent smoke path so the published helper entry does not silently rot.
13. When a coverage gap is marked closed, make the CTest registration, catalog entry, and exact passing command visible in the owning ledger or checkpoint document.
14. New syntax surfaces need focused lexer/parser coverage plus the smallest runtime smoke that proves any supported lowering path.
15. When standard-stream syntax changes, include both parser-only shorthand coverage and a runtime stdin/stdout smoke so symbolic declarations cannot parse while the executable path stays broken.
16. When generic/container function type annotations change, cover both parser-route acceptance and a lowering/codegen case for the smallest supported runtime family, so `list[T]` or `dict[K,V]` annotations cannot parse while call lowering regresses.
17. When a collection annotation adds contextual validation, pair the positive runtime smoke with a negative semantic test and an untyped-control case proving ordinary nested lists keep their prior behavior.
18. When control-flow spellings change, keep feature stdout goldens and security/codegen regressions together: `^...` must prove nearest-loop behavior, and nested `<| expr` returns must prove they exit the enclosing function.
19. When a syntax revision retires old retired syntax, delete the active `.styio` fixture and golden instead of marking it expected-red. Then remove the `TEST-CATALOG` row, add a revision note to the feature-test/design docs, and rerun the affected label plus `ctest -L language_feature`.
20. Native interop acceptance must include parser-only top-level guards and executable feature goldens that prove C/C++ source is compiled, linked, loaded, and called through the JIT.
21. When tests create custom AST nodes or compiler-stage visitors, use the split visitor signatures: `typeInfer(StyioSemaContext*)` and `toStyioIR(AstToStyioIRLowerer*)`.
22. Put C++ reference equivalence cases under `tests/algorithms/<case>/`; keep the C++ oracle, Styio program, and per-case random-input test driver in that directory, with only shared runner code under `tests/algorithms/.common/`.
23. When post-push CI reports five-layer typed-AST or diagnostic expectation drift, rebuild the local test binary before trusting a prior pass, reproduce the exact failing CTest filters, then update only the stale golden or stable diagnostic fragment.
24. Syntax aliases that assert canonical equivalence need both runtime equivalence and exact lowered or LLVM IR comparison where the backend contract is part of the statement; include at least one non-example-shaped case so optimizer coverage cannot be a one-off source rewrite.
25. Internal resource declarations need parser coverage for the prelude source file plus negative tests for undeclared local names and not-allowed hidden pseudo-primitives such as `file(path)`.
26. Task_resource syntax needs both positive stdout goldens and semantic negatives: cover `answer <- job`, `job -> answer -> @stdout`, string and numeric results, undeclared flow targets, and double-pull rejection in the same feature registration.
27. Profiler changes must keep `styio_profiler_frontend_smoke` on a task_using fixture and assert the JSON keys that prove scheduler counters and expanded phase names are wired, not just that a profile file exists. Native executable profiling changes must keep `styio_build_native_executable_stdin_echo` green and preserve opt-in `STYIO_NATIVE_PROFILE_OUT` behavior.
28. Expression-oriented statement semantics need one runtime smoke that covers function match sugar, a block final expression returning from a function, a match-arm final expression returning from a branch, and a statement-only tail returning the default value.
29. Resource-topology safety tests live in `tests/resource_topology_test.cpp`. They must cover capability rejection, close-capable ownership, stream backpressure edges, hidden-ledger scope, and handle-table release/recycle before a resource lifecycle change is considered accepted.
30. state-resource retirement coverage keeps positive feature fixtures on Topology v2 syntax and preserves retired state-family spellings only as registered negative tests with stable migration diagnostics.
31. Native executable artifact coverage must build through `styio build <file_path> -o <artifact_name>`, assert the produced file is executable, and run the artifact against an existing golden so the test proves both artifact creation and runtime behavior.
32. Resource method tests must cover static method resolution, consuming receiver invalidation, transitive consuming method calls, final binding override rejection, property-as-method rejection, method arity rejection, repeated consuming call rejection, non-consuming overrides that must not lower to release, task outer-resource consume rejection, explicit `=>` ordering for exclusive borrows, and lowering evidence for file `write`/`close` methods before the topology model is considered regression-covered.
33. README showcase examples that are wired into CTest must run repository-local Styio source from the repository root and compare stdout against a checked-in golden, so public examples cannot drift away from executable compiler behavior.
34. Semantic negative tests must assert a stable diagnostic fragment from `tests/features/<feature>/expected/*.err`; a nonzero exit code alone is not enough evidence.
35. Lit/FileCheck-style fixture trees belong under active `tests/` only when they are registered in CTest and have real check lines. Otherwise archive them until a live runner owns them.
36. LibFuzzer runtime probes must compile a minimal `LLVMFuzzerTestOneInput` entrypoint. Do not probe `-fsanitize=fuzzer` with a custom `main`, because the sanitizer runtime provides `main` and the check will fail for the wrong reason.
37. Fuzz targets that exercise tokenizer, parser, AST, or compiler session objects must run each input inside `CompilationSession` or an equivalent arena owner so sanitizer deep runs catch real memory bugs instead of expected session-lifetime allocations.
38. When replacing a placeholder with accepted behavior, pair the positive fixture with the smallest semantic negative that proves adjacent undefined syntax fails closed; for format strings this means a real `$"..."` stdio-output smoke plus a hash-tag iterator sequence diagnostic.
39. Nightly fuzz leak artifacts must become durable regression evidence. Add each minimized lexer/parser seed to `tests/fuzz/corpus/`, add the smallest security or ASan-targeted regression that exercises the same recovery path, preserve embedded NUL bytes in deterministic parser regression inputs when the artifact has them, and record any local libFuzzer toolchain blocker separately from the code fix.
40. Parser lifetime regressions that pass through typed annotations need both routes covered when the seed can reach them. Keep the raw fuzz corpus byte-for-byte, then add a deterministic security test that builds the same embedded-NUL input through `CompilationSession` so ASan/LSan validates parser recovery ownership without depending on the local libFuzzer runtime.
41. Parser DoS or OOM fuzz artifacts follow the same evidence path as lifetime bugs. Preserve the raw seed byte-for-byte, add a deterministic resource-limit regression, replay the artifact and the tracked corpus seed with the local libFuzzer target, and verify the relevant security tests under both default and ASan builds.
42. Parser timeout artifacts caused by nightly-to-legacy bridge loops need explicit closure evidence. Keep the minimized seed in `tests/fuzz/corpus/parser/`, add a deterministic security regression that exercises the same malformed nest through `CompilationSession`, replay the isolated artifact, and replay the full parser corpus so a single fixed seed does not hide another fallback loop nearby.
43. When fuzz minimizes a previously tracked parser timeout into a smaller bridge-loop seed, keep the derivative corpus file too. Add a second deterministic regression for the smaller shape and prove that both the original artifact and the minimized derivative replay cleanly, or bridge-budget fixes can look closed while a nearby cursor-starter path still times out.
44. Leak artifacts from parser fuzzing need session-backed regressions, not just direct parser helper calls. If the parser runs under `CompilationSession` arenas, reproduce the exact seed bytes through the session path, replay the isolated `leak-*` artifact with leak detection enabled, and keep the raw corpus seed so later timeout fixes do not leave an exception-path destructor leak behind.

## Change Classes

1. Small: new fixture for already accepted behavior, expected-output fix, or test naming cleanup. Run targeted test.
2. Medium: new feature-test area, five-layer case, security regression, parser shadow gate update, or compile-plan artifact assertion expansion. Update docs and run affected labels.
3. High: new test framework, changed oracle policy, fuzz corpus backflow, or checkpoint-health gate change. Use checkpoint workflow and add ADR if the gate becomes required.

## Required Gates

Common commands:

```bash
ctest --test-dir build/default -L language_feature
ctest --test-dir build/default -L styio_pipeline
ctest --test-dir build/default -L security
ctest --test-dir build/default -L resource_topology
ctest --test-dir build/default -R '^parser_shadow_gate_'
ctest --test-dir build/default -L algorithm_equivalence
```

Fuzz smoke:

```bash
ctest --test-dir build/fuzz -L fuzz_smoke
```

`fuzz_smoke` 当前走独立 corpus-replay smoke binaries，而不是直接把 PR 门禁绑在 libFuzzer main 的启动行为上；真正的 libFuzzer 目标仍保留给手动/夜间深跑。

Docs and recovery:

```bash
python3 tests/workflow_scheduler_test.py
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
./scripts/checkpoint-health.sh --no-asan
```

`checkpoint-health.sh` is allowed to reconfigure the requested build dir; maintenance changes to that recovery path must preserve a clean build-dir handoff instead of leaking configure logs into later commands. The default local variant is `build/default/`; use `--build-dir build/<variant>` for another configured variant.
同一脚本在 normal leg 里必须显式构建 `styio_security_test` 后再跑 `ctest -L security`；空标签返回 0 不能算通过。
离线恢复时，`tests/CMakeLists.txt` 和顶层 `CMakeLists.txt` 现在会优先复用本地已有的 `googletest` / `tree_sitter_runtime` source checkout，避免首次恢复因 FetchContent 远端不可达而卡死。

## Cross-Team Dependencies

1. Frontend must review parser, lexer, and shadow gate expectations.
2. Sema / IR must review AST, type, lowering, and repr goldens.
3. Codegen / Runtime must review LLVM, runtime, security, and soak expectations.
4. Perf / Stability must review benchmark or soak threshold changes.
5. Docs / Ecosystem must review test catalog and workflow documentation changes.

## Handoff / Recovery

Record unfinished quality work with:

1. Test name, label, and fixture path.
2. Input and oracle path.
3. Whether failure is expected-red or unexpected regression.
4. Owning implementation team.
5. Required team runbook when the team-docs gate fails.
6. Exact command that reproduces the failure.
