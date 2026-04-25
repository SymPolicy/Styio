# Test Quality Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of milestone tests, golden files, five-layer pipeline cases, security tests, fuzz smoke, parser shadow gates, and test documentation.

**Last updated:** 2026-04-26

## Mission

Own the evidence that Styio behavior is accepted, reproducible, and recoverable. This team protects CTest registration, fixture layout, golden oracles, fuzz/security coverage, and test catalog accuracy. It does not decide language semantics without the design SSOT.

## Owned Surface

Primary paths:

1. `tests/`
2. `tests/CMakeLists.txt`
3. `tests/fuzz/`
4. `tests/security/`
5. `src/StyioTesting/`
6. [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md)
7. [../assets/workflow/FIVE-LAYER-PIPELINE.md](../assets/workflow/FIVE-LAYER-PIPELINE.md)
8. [../assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md](../assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md)

## Daily Workflow

1. Identify the behavior owner before adding an oracle.
2. Choose the smallest useful test layer: milestone stdout, semantic failure, five-layer, C++ unit, security, fuzz, shadow gate, or soak.
3. Register every new automated test in CMake.
4. Update [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) when adding or changing acceptance tests.
5. Keep generated or temporary outputs out of the repository unless the test framework explicitly treats them as goldens.
6. Treat compile-plan negative-path coverage and machine-readable diagnostics as contract evidence, not optional smoke coverage.
7. When compile-plan artifacts grow, add assertions for receipt fields and auxiliary artifacts such as `runtime-events.jsonl`, not just exit codes.
8. Keep five-layer Layer 4 LLVM goldens semantic, not implementation-bound: when stdout lowering moves between legacy `printf/puts` and runtime helpers such as `styio_stdout_write_cstr`, or when LLVM stops printing unused `declare` lines and renumbers transient `%<n>` temporaries, update the pipeline canonicalization before touching large golden sets.

## Change Classes

1. Small: new fixture for already accepted behavior, expected-output fix, or test naming cleanup. Run targeted test.
2. Medium: new milestone area, five-layer case, security regression, parser shadow gate update, or compile-plan artifact assertion expansion. Update docs and run affected labels.
3. High: new test framework, changed oracle policy, fuzz corpus backflow, or checkpoint-health gate change. Use checkpoint workflow and add ADR if the gate becomes required.

## Required Gates

Common commands:

```bash
ctest --test-dir build -L milestone
ctest --test-dir build -L styio_pipeline
ctest --test-dir build -L security
ctest --test-dir build -R '^parser_shadow_gate_'
```

Fuzz smoke:

```bash
ctest --test-dir build-fuzz -L fuzz_smoke
```

`fuzz_smoke` 当前走独立 corpus-replay smoke binaries，而不是直接把 PR 门禁绑在 libFuzzer main 的启动行为上；真正的 libFuzzer 目标仍保留给手动/夜间深跑。

Docs and recovery:

```bash
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
./scripts/checkpoint-health.sh --no-asan
```

`checkpoint-health.sh` is allowed to reconfigure the requested build dir and fall back from `build/` to `build-codex/`; maintenance changes to that recovery path must preserve a clean build-dir handoff instead of leaking configure logs into later commands.
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
