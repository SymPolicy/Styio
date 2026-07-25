# Codegen / Runtime Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of LLVM codegen, JIT integration, external runtime helpers, handle tables, and runtime safety contracts.

**Last updated:** 2026-07-20

## Mission

Own StyioIR-to-LLVM lowering and the runtime surface that compiled programs call. This team protects LLVM IR correctness, JIT symbol exposure, external helper ownership, handle lifecycle, runtime diagnostics, and performance-sensitive execution paths.

## Owned Surface

Primary paths:

1. `src/StyioCodeGen/`
2. `src/StyioJIT/`
3. `src/StyioNative/`
4. `src/StyioExtern/`
5. `src/StyioRuntime/`
6. Runtime-facing parts of `src/main.cpp`

Related docs:

1. [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md)
2. [../design/Styio-StdLib-Intrinsics.md](../design/Styio-StdLib-Intrinsics.md)
3. [../../workflows/FIVE-LAYER-PIPELINE.md](../../workflows/FIVE-LAYER-PIPELINE.md)
4. [../../workflows/ADD-SYNTAX-WITH-SKILLS.md](../../workflows/ADD-SYNTAX-WITH-SKILLS.md)

## Daily Workflow

1. Confirm the incoming StyioIR shape before changing LLVM emission.
2. Keep runtime helper ownership explicit, especially for strings, handles, and file resources.
3. Treat diagnostic code, exit code, and runtime error category changes as public behavior.
4. Update security, five-layer, and soak coverage before accepting a runtime contract change.
5. Use benchmark routes for hot paths, not terminal timing impressions.
6. Preserve the current legacy bounded final-bind compatibility contract until an explicit final-binding/Topology checkpoint changes it: final-bind lowers to `[n x i64] + head`, reads return the latest slot, same-name flex after final bind is rejected, and function-parameter ring semantics remain incomplete.
7. Treat `runtime-events.jsonl` as a published artifact: changes to `compile.* / run.* / thread.* / unit.* / unit.test.* / state.* / transition.fired / log.emitted / diagnostic.emitted` require same-checkpoint tests and consumer doc updates.
8. Keep `stdout/stderr` helper hooks lossless: runtime log replay may enrich the artifact stream, but must not change observable program output semantics.
9. Keep the ORC JIT symbol registry aligned with the full `src/StyioExtern/ExternLib.hpp` export surface and every runtime helper that codegen emits; when a new `getOrInsertFunction("styio_*")` call or extern export appears, update `src/StyioJIT/StyioJIT_ORC.hpp` in the same delivery.
10. Treat `python3 scripts/runtime-surface-gate.py` as the static blocker for syntax/runtime deliveries; do not rely on manual review to spot a missing export or ORC registration.
11. Keep native extern JIT registration intact when resolving upstream merges: `StyioJIT_ORC::defineAbsoluteSymbol` is the bridge used by `StyioCodeGen` to expose compiled C/C++ extern blocks to ORC, and absolute symbols must stay both `Callable` and `Exported` so native interop lookup remains stable.
12. Matrix runtime helpers own the flat row-major storage contract. When adding or changing matrix lowering, keep `ExternLib.hpp`, `ExternLib.cpp`, `HandleTable.hpp`, ORC registrations, direct-data helpers, release paths, and security/codegen tests in the same checkpoint.
13. Empty lexical scopes must not emit unused runtime declarations; exact LLVM IR comparison tests protect StyioIR optimizer canonicalization from backend-only drift.
14. Matrix/list/dict/string runtime resources stored in dynamic slots must release through the same RAII path on overwrite, normal scope exit, and runtime-error early return. Any new runtime guard that emits `ret` must first run active scope cleanup.
15. Task resources are scheduled native handles. Keep `styio_task_*_spawn`, worker-pool state, `HandleKind::Task`, dynamic-slot release, ORC registrations, and task-operation codegen in one checkpoint; `||>` lowering must emit a private task function plus scheduler submission, not an eager scalar handle that can escape scope cleanup. Generic value settlement such as `answer = ?| task_operation | fallback` directly dispatches the verified finite nominal completion ID: only the selected recovery runs once, only matched families are consumed, and remaining families propagate through the caller's statically verified completion convention. Do not materialize or clear an ambient task error, insert a runtime error guard, search handlers dynamically, or retry. A composed `?| (task_operation -> destination) | fallback` still lowers `->` as the same left-to-right data transfer used by every endpoint family; successful transfer is Unit, while destination type and capability choose the concrete store/consume path without giving the arrow a task-specific meaning. The current typed await-target route `?| task -> value: T | fallback` is migration debt and must be deleted rather than preserved as a backend contract.
16. Async scheduler profiling must stay opt-in: disabled runs should avoid per-task counter writes, enabled runs should expose spawn/enqueue/start/complete/pull/release and queue-depth counters through `--profile-frontend`, and task readiness should use the scheduler's low-overhead atomic wait path instead of per-task condition variables.
17. Function and match-branch result semantics come from explicit Block-completion IR emitted by lowering. A reachable function-body fallthrough produces the unique Unit value; a non-Unit signature without a compatible explicit Block result is a Sema/IR verification error. LLVM codegen must never fabricate a runtime default or reuse the last emitted statement temporary.
18. File-resource destroy lowering must use explicit handle release IR. `SIOHandleRelease` should call `styio_file_close`, clear named handle slots to zero so scope-exit cleanup is idempotent, and preserve `@("path").close()` direct-path behavior without bypassing runtime diagnostics.
19. Typed stdin pulls lower from `InstantPull` to `SIOStdStreamPull::result_type` for scalar i64/f64/string and typed list pulls. Untyped collect-bind stdin may still use `SIOListReadStdin`. String pulls must clone the borrowed stdin buffer before binding, and `list[f64]` stdin pulls must use the f64 list reader rather than falling through to i64 parsing. String concatenation should route non-string operands through `promote_to_cstr` so f64 formatting uses the same runtime decimal helper as other output paths.
20. Dynamic-slot stores must fail closed on mismatched LLVM value families. Do not replace invalid integer, floating, or pointer fields with zero/null sentinels unless the IR node explicitly represents an undefined value.
21. Internal IR operator dispatch must fail closed. Unknown binary or logical operators are typed diagnostics, not zero/left-operand fallbacks, and each new operator family needs a focused security/codegen regression before it can reach LLVM emission.
22. Native interop platform compatibility belongs with runtime ownership: keep dynamic-library load/unload/symbol lookup paths portable across `dlopen` and Windows `LoadLibrary`, and pair loader changes with the smallest native interop or LSP build smoke that exercises the affected binary.
23. Standalone continue codegen targets the innermost active loop. Do not reintroduce multi-depth continue dispatch in LLVM emission unless Sema and IR grow a new explicit continuation-domain contract first.

24. `Q02-INF` codegen consumes only Sema-proven concrete callable instances. Key and reuse every instance by the canonical concrete type vector plus a stable source-definition identity; generated names must be deterministic across call order, incremental scopes, and hash iteration. Enforce recursive-instance growth and code-size gates before emission, and reuse an existing instance instead of recompiling the same vector. There is no runtime generic dictionary, dynamic type dispatch, unresolved type variable, first-call specialization policy, or backend `i64` repair path. LLVM must reject any SGIR that still carries unsolved `forall`/`Literal`/`Add` metadata. The approved `Q05-LIT-ADD` relation in item 25 is delivered as solved Sema facts; codegen must not derive a competing table from convenient LLVM operators.
25. `Q05-LIT-ADD` is design-approved and implementation-pending. Emit only the concrete same-type scalar rows selected by Sema. Signed `i8`–`i128` addition uses checked upper/lower-bound arithmetic and routes overflow to the no-payload nominal `overflow` completion; compile-known overflow must use the same control edge, not wrap or become a backend-only trap/error. `f32`/`f64` addition must preserve the fixed IEEE ties-to-even, gradual-underflow, subnormal, signed-zero, infinity, and NaN result domain without reading host rounding state or enabling fast-math, FTZ/DAZ, or reassociation that changes results. Constant folding and emitted execution must agree bit-for-semantics and completion-for-completion. No runtime generic dictionary or fallback conversion is introduced. Operand readiness, completion stop, and commit points consume the accepted Q03-F graph; current lowering order, LLVM instruction order, or host evaluation order is never language authority. Conversion, other operators, aliases/unsigned types, string/container/matrix rows, and NaN payload/equality/order remain open Q05 work.
26. `Q03-F` is design-approved and implementation-pending. Lower only verified typed evaluation DAG/CFG facts: obtain every receiver, argument, source, and endpoint once into a stable value reference; preserve short-circuit/match/settlement branch laziness; stop later Block items after completion; execute mandatory lexical exit obligations; publish only after their settlement; and preserve logical resource/effect order while allowing separately proven physical fusion. The source and endpoint of `->` are independent prerequisites, and the arrow only fixes data direction. Replace ambient global runtime-error polling with the bounded explicit Outcome ABI owned by the Q03-F plan; introduce no heap exception object, dynamic handler stack, hidden transaction log, retry, or rollback. Codegen may exercise each of the four optimization rights only when the verifier supplied that exact right.
27. `styio_list_stride(handle, stride)` returns a newly owned list containing zero-based positions `0, stride, ...`, clones selected nested handles, and reports non-positive dynamic strides through the bounds runtime error family. Codegen must consume a tracked temporary source, track the returned list, and keep declaration, ORC registration, and runtime-surface evidence synchronized.

## Change Classes

1. Small: local LLVM builder cleanup or helper refactor with unchanged IR output. Run targeted pipeline tests.
2. Medium: changed LLVM IR shape, runtime helper behavior, extern symbol, or diagnostic mapping. Run five-layer, security, and affected feature labels.
3. High: handle table, ownership lifecycle, JIT symbol policy, runtime event sink, or resource/stream execution behavior. Use checkpoint workflow, add ADR, and run soak/perf gates.

## Required Gates

Minimum local commands:

```bash
python3 scripts/runtime-surface-gate.py
ctest --test-dir build/default -L styio_pipeline
ctest --test-dir build/default -L security
ctest --test-dir build/default -L language_feature
```

Runtime stability:

```bash
ctest --test-dir build/default -L soak_smoke
STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark ./benchmark/perf-route.sh --quick
```

For deeper runtime or allocation work:

```bash
ctest --test-dir build/default -L soak_deep
STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark \
  ./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20
```

## Cross-Team Dependencies

1. Sema / IR must review every changed IR input contract.
2. Test Quality must review five-layer or security golden updates.
3. Perf / Stability must review benchmark matrix, RSS thresholds, or long-loop behavior.
4. CLI / Nano must review runtime capability output exposed through machine-info.
5. `pafio` / `view` consumers must review published runtime-event family additions or payload-shape changes.

## Handoff / Recovery

Record unfinished codegen/runtime work with:

1. IR node or runtime helper involved.
2. LLVM IR before/after expectation.
3. Runtime symbol or handle lifecycle state.
4. Failing security, pipeline, soak, or benchmark command.
5. Known rollback point and whether generated goldens are intentionally stale.
6. Runtime-event family changes and the exact consumer docs/gates updated with them.
