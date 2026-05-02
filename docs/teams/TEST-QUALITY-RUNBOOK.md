# Test Quality Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of milestone tests, golden files, five-layer pipeline cases, security tests, fuzz smoke, parser shadow gates, and test documentation.

**Last updated:** 2026-05-03

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
2. Choose the smallest useful test layer: milestone stdout, semantic failure, five-layer, C++ unit, security, fuzz, shadow gate, or soak.
3. Register every new automated test in CMake.
4. Update [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) when adding or changing acceptance tests.
5. Keep generated or temporary outputs out of the repository unless the test framework explicitly treats them as goldens.
6. Treat compile-plan negative-path coverage and machine-readable diagnostics as contract evidence, not optional smoke coverage.
7. When compile-plan artifacts grow, add assertions for receipt fields and auxiliary artifacts such as `runtime-events.jsonl`, not just exit codes.
8. Keep five-layer Layer 4 LLVM goldens semantic, not implementation-bound: when stdout lowering moves between legacy `printf/puts` and runtime helpers such as `styio_stdout_write_cstr`, or when LLVM stops printing unused `declare` lines and renumbers transient `%<n>` temporaries, update the pipeline canonicalization before touching large golden sets.
9. Treat workflow scheduler tests as gate-level regression coverage; changes to scheduler profiles, phase ordering, or registry validation must update `tests/workflow_scheduler_test.py`.
10. When compiler handoff contracts grow, add or update regression coverage for both `--machine-info=json` and `--source-build-info=json` so `spio`-facing metadata cannot drift silently.
11. When the compiler-side source-build helper changes, keep a lightweight regression on `scripts/source-build-minimal.sh --help` or an equivalent smoke path so the published helper entry does not silently rot.
12. When a coverage gap is marked closed, make the CTest registration, catalog entry, and exact passing command visible in the owning ledger or checkpoint document.
13. New syntax surfaces need focused lexer/parser coverage plus the smallest runtime smoke that proves any supported lowering path.
14. When standard-stream syntax changes, include both parser-only shorthand coverage and a runtime stdin/stdout smoke so symbolic declarations cannot parse while the executable path stays broken.
15. When generic/container function type annotations change, cover both parser-route acceptance and a lowering/codegen case for the smallest supported runtime family, so `list[T]` or `dict[K,V]` annotations cannot parse while call lowering regresses.
16. When a collection annotation adds contextual validation, pair the positive runtime smoke with a negative semantic test and an untyped-control case proving ordinary nested lists keep their prior behavior.
17. When control-flow spellings change, keep milestone stdout goldens and security/codegen regressions together: `^...` must prove nearest-loop behavior, and nested `<| expr` returns must prove they exit the enclosing function.
18. When a syntax revision retires old milestone syntax, delete the active `.styio` fixture and golden instead of marking it expected-red. Then remove the `TEST-CATALOG` row, add a revision note to the milestone/design docs, and rerun the affected label plus `ctest -L milestone`.
19. Native interop acceptance must include parser-only top-level guards and executable milestone goldens that prove C/C++ source is compiled, linked, loaded, and called through the JIT.
20. When tests create custom AST nodes or compiler-stage visitors, use the split visitor signatures: `typeInfer(StyioSemaContext*)` and `toStyioIR(AstToStyioIRLowerer*)`.
21. Put C++ reference equivalence cases under `tests/algorithms/<case>/`; keep the C++ oracle, Styio program, and per-case random-input test driver in that directory, with only shared runner code under `tests/algorithms/.common/`.
22. When post-push CI reports five-layer typed-AST or diagnostic expectation drift, rebuild the local test binary before trusting a prior pass, reproduce the exact failing CTest filters, then update only the stale golden or stable diagnostic fragment.

## Change Classes

1. Small: new fixture for already accepted behavior, expected-output fix, or test naming cleanup. Run targeted test.
2. Medium: new milestone area, five-layer case, security regression, parser shadow gate update, or compile-plan artifact assertion expansion. Update docs and run affected labels.
3. High: new test framework, changed oracle policy, fuzz corpus backflow, or checkpoint-health gate change. Use checkpoint workflow and add ADR if the gate becomes required.

## Required Gates

Common commands:

```bash
ctest --test-dir build/default -L milestone
ctest --test-dir build/default -L styio_pipeline
ctest --test-dir build/default -L security
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
