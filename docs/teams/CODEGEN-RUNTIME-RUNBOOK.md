# Codegen / Runtime Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of LLVM codegen, JIT integration, external runtime helpers, handle tables, and runtime safety contracts.

**Last updated:** 2026-05-03

## Mission

Own StyioIR-to-LLVM lowering and the runtime surface that compiled programs call. This team protects LLVM IR correctness, JIT symbol exposure, external helper ownership, handle lifecycle, runtime diagnostics, and performance-sensitive execution paths.

## Owned Surface

Primary paths:

1. `src/StyioCodeGen/`
2. `src/StyioJIT/`
3. `src/StyioExtern/`
4. `src/StyioRuntime/`
5. Runtime-facing parts of `src/main.cpp`

Related docs:

1. [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md)
2. [../design/Styio-StdLib-Intrinsics.md](../design/Styio-StdLib-Intrinsics.md)
3. [../assets/workflow/FIVE-LAYER-PIPELINE.md](../assets/workflow/FIVE-LAYER-PIPELINE.md)
4. [../assets/workflow/SYNTAX-ADDITION-WORKFLOW.md](../assets/workflow/SYNTAX-ADDITION-WORKFLOW.md)

## Daily Workflow

1. Confirm the incoming StyioIR shape before changing LLVM emission.
2. Keep runtime helper ownership explicit, especially for strings, handles, and file resources.
3. Treat diagnostic code, exit code, and runtime error category changes as public behavior.
4. Update security, five-layer, and soak coverage before accepting a runtime contract change.
5. Use benchmark routes for hot paths, not terminal timing impressions.
6. Preserve the current partial `[|n|]` bounded-ring contract until an explicit M8/Topology checkpoint changes it: final-bind lowers to `[n x i64] + head`, reads return the latest slot, same-name flex after final bind is rejected, and function-parameter ring semantics remain incomplete.
7. Treat `runtime-events.jsonl` as a published artifact: changes to `compile.* / run.* / thread.* / unit.* / unit.test.* / state.* / transition.fired / log.emitted / diagnostic.emitted` require same-checkpoint tests and consumer doc updates.
8. Keep `stdout/stderr` helper hooks lossless: runtime log replay may enrich the artifact stream, but must not change observable program output semantics.
9. Keep the ORC JIT symbol registry aligned with the full `src/StyioExtern/ExternLib.hpp` export surface and every runtime helper that codegen emits; when a new `getOrInsertFunction("styio_*")` call or extern export appears, update `src/StyioJIT/StyioJIT_ORC.hpp` in the same delivery.
10. Treat `python3 scripts/runtime-surface-gate.py` as the static blocker for syntax/runtime deliveries; do not rely on manual review to spot a missing export or ORC registration.
11. Keep native extern JIT registration intact when resolving upstream merges: `StyioJIT_ORC::defineAbsoluteSymbol` is the bridge used by `StyioCodeGen` to expose compiled C/C++ extern blocks to ORC, and it must stay aligned with native interop tests.
12. Matrix runtime helpers own the flat row-major storage contract. When adding or changing matrix lowering, keep `ExternLib.hpp`, `ExternLib.cpp`, `HandleTable.hpp`, ORC registrations, direct-data helpers, release paths, and security/codegen tests in the same checkpoint.

## Change Classes

1. Small: local LLVM builder cleanup or helper refactor with unchanged IR output. Run targeted pipeline tests.
2. Medium: changed LLVM IR shape, runtime helper behavior, extern symbol, or diagnostic mapping. Run five-layer, security, and affected milestones.
3. High: handle table, ownership lifecycle, JIT symbol policy, runtime event sink, or resource/stream execution behavior. Use checkpoint workflow, add ADR, and run soak/perf gates.

## Required Gates

Minimum local commands:

```bash
python3 scripts/runtime-surface-gate.py
ctest --test-dir build/default -L styio_pipeline
ctest --test-dir build/default -L security
ctest --test-dir build/default -L milestone
```

Runtime stability:

```bash
ctest --test-dir build/default -L soak_smoke
./benchmark/perf-route.sh --quick
```

For deeper runtime or allocation work:

```bash
ctest --test-dir build/default -L soak_deep
./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20
```

## Cross-Team Dependencies

1. Sema / IR must review every changed IR input contract.
2. Test Quality must review five-layer or security golden updates.
3. Perf / Stability must review benchmark matrix, RSS thresholds, or long-loop behavior.
4. CLI / Nano must review runtime capability output exposed through machine-info.
5. `spio` / `view` consumers must review published runtime-event family additions or payload-shape changes.

## Handoff / Recovery

Record unfinished codegen/runtime work with:

1. IR node or runtime helper involved.
2. LLVM IR before/after expectation.
3. Runtime symbol or handle lifecycle state.
4. Failing security, pipeline, soak, or benchmark command.
5. Known rollback point and whether generated goldens are intentionally stale.
6. Runtime-event family changes and the exact consumer docs/gates updated with them.
