# Codegen / Runtime Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of LLVM codegen, JIT integration, external runtime helpers, handle tables, and runtime safety contracts.

**Last updated:** 2026-05-30

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
3. [../../workflows/FIVE-LAYER-PIPELINE.md](../../workflows/FIVE-LAYER-PIPELINE.md)
4. [../../workflows/ADD-SYNTAX-WITH-SKILLS.md](../../workflows/ADD-SYNTAX-WITH-SKILLS.md)

## Daily Workflow

1. Confirm the incoming StyioIR shape before changing LLVM emission.
2. Keep runtime helper ownership explicit, especially for strings, handles, and file resources.
3. Treat diagnostic code, exit code, and runtime error category changes as public behavior.
4. Update security, five-layer, and soak coverage before accepting a runtime contract change.
5. Use benchmark routes for hot paths, not terminal timing impressions.
6. Preserve the current legacy bounded final-bind compatibility contract until an explicit final-binding/Topology checkpoint changes it: legacy bounded rings lower to `[n x i64] + head`, reads return the latest slot, same-name flex after final bind is rejected, and function-parameter ring semantics remain incomplete. Typed Topology rings such as bounded `f64`, bounded `bool`, bounded `char`, bounded `string`, bounded `list`, bounded `dict`, and bounded `matrix` must derive storage behavior from the ring data type instead of reintroducing an unconditional `i64` ring assumption. Char rings store `i8` values; string rings must clone values into ring-owned cstrs, return cloned cstrs to consumers, and release ring-owned cstrs on overwrite and scope cleanup; list/dict/matrix rings store runtime handles as `i64`, clone handles on write/read, move pending handles into committed slots, and release ring-owned handles on overwrite and scope cleanup.
7. Treat `runtime-events.jsonl` as a published artifact: changes to `compile.* / run.* / thread.* / unit.* / unit.test.* / state.* / transition.fired / log.emitted / diagnostic.emitted` require same-checkpoint tests and consumer doc updates.
8. Keep `stdout/stderr` helper hooks lossless: runtime log replay may enrich the artifact stream, but must not change observable program output semantics.
9. Keep the ORC JIT symbol registry aligned with the full `src/StyioExtern/ExternLib.hpp` export surface and every runtime helper that codegen emits; when a new `getOrInsertFunction("styio_*")` call or extern export appears, update `src/StyioJIT/StyioJIT_ORC.hpp` in the same delivery.
10. Treat `python3 scripts/runtime-surface-gate.py` as the static blocker for syntax/runtime deliveries; do not rely on manual review to spot a missing export or ORC registration.
11. Keep native extern JIT registration intact when resolving upstream merges: `StyioJIT_ORC::defineAbsoluteSymbol` is the bridge used by `StyioCodeGen` to expose compiled C/C++ extern blocks to ORC, and it must stay aligned with native interop tests.
12. Matrix runtime helpers own the flat row-major storage contract. When adding or changing matrix lowering, keep `ExternLib.hpp`, `ExternLib.cpp`, `HandleTable.hpp`, ORC registrations, direct-data helpers, clone helpers such as `styio_matrix_clone_i64` / `styio_matrix_clone_f64`, release paths, and security/codegen tests in the same checkpoint.
13. Empty lexical scopes must not emit unused runtime declarations; exact LLVM IR comparison tests protect StyioIR optimizer canonicalization from backend-only drift.
14. Matrix/list/dict/string runtime resources stored in dynamic slots must release through the same RAII path on overwrite, normal scope exit, and runtime-error early return. Any new runtime guard that emits `ret` must first run active scope cleanup. Explicit `SGReturn` must not bypass tracked file cleanup: close active file-handle slots before emitting the LLVM return, then use the post-cleanup runtime-error guard so cleanup failures settle without recursively running cleanup again. Loop `^` break and standalone `>>` continue lowering must clean the resource scopes they bypass before branching to the loop target, while preserving metadata-only scope unwinding for terminated blocks so sibling branches still codegen against the correct lexical scope.
15. Task resources are scheduled runtime handles. Keep `styio_task_*_spawn`, worker-pool state, `HandleKind::Task`, dynamic-slot release, ORC registrations, and task pull codegen in one checkpoint; `||>` lowering must emit a private task function plus scheduler submission, not an eager scalar handle that can escape scope cleanup. `?| task -> value: T | fallback` uses the same task pull path, clears runtime task errors before evaluating fallback, and without fallback must raise the runtime error guard immediately at the settlement site. Resource operations follow the same runtime boundary: `?| resource_operation` settles in place, while `?| resource_operation | fallback` may run fallback code only after the resource failure is materialized and cleared according to the typed failure contract.
16. Plain file acquire, write, release, line-iterator operations, acquired-handle instant pulls, and materialized list/dict/matrix index, list-slice, ordered dict value-slice, or matrix row-range slice reads outside `SIOResourceEffect` must emit the default runtime guard at the operation boundary so a materialized runtime error cannot leak into later statements. File line iteration must keep EOF separate from runtime failure: guard direct `@file(...) >> #(line)` open failures, and when `styio_file_read_line` returns null, inspect `styio_runtime_has_error()` before treating it as EOF. Acquired-handle file instant pulls use `styio_file_read_i64line_from_handle` so EOF returns `0` like path-backed file instant pull while zero or stale handles preserve `STYIO_RUNTIME_INVALID_FILE_HANDLE` for `closed` handlers. `SCListGet`, `SCListSlice`, `SCDictGet`, ordered dict value slices lowered as `SCDictValues` plus `SCListSlice`, `SCMatrixGet`, `SCMatrixRow`, and `SCMatrixRowsSlice` must guard `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, or `STYIO_RUNTIME_MATRIX_INDEX` outside `SIOResourceEffect` after releasing owned temporaries. While lowering a `SIOResourceEffect`, suppress operation-local default guards and let the wrapper inspect `styio_runtime_has_error()` so catch-all fallback, named handlers, no-fallback settlement, and discard keep their explicit recovery semantics. This includes `SIOHandleAcquire` for statement-shaped acquire `?| f <- @file(...) | fallback` and statement-shaped file rebind `?| f = @file(...) | fallback`, `SIOHandleRelease` emitted by resource method calls such as `?| @file(...).close() | fallback`, `SIOResourceWriteToFile` emitted for acquired-handle file `write` methods such as `?| f.write(...) | ...`, and `SIOInstantPull` from acquired handles such as `?| (<< f) | closed => value`: successful open/close/write/read skips recovery, file-open read failures dispatch through `io`, file-rebind prior-handle cleanup failures branch to the wrapper before replacement open, zero acquired handles must materialize `STYIO_RUNTIME_INVALID_FILE_HANDLE` and dispatch through `closed` when matched, guarded writes must not create the path after a recovered failed acquire, and no-fallback settlement still raises before later statements. For simple user-defined resource methods whose body is a single `<| expr`, lowering must extract the returned expression before StyioIR generation so direct calls and `SIOResourceEffect` value paths produce the expression value instead of emitting an expression-context `SGReturn`; method-body inline cloning must preserve scalar leaves, `FmtStrAST`, `InstantPullAST`, `RangeAST`, `MatchCasesAST`, `FuncCallAST`, value-producing `ResourceEffectAST` result types, `ListAST`, `ListOpAST`, `DictAST`, and matrix `ListOpAST` substitutions so returned bool/f64/char/string, format-string, scalar/string match, and returned block-form function-call values preserve their result family, returned statement-only function calls and returned container match results remain fail-closed, returned resource-effect expressions return their own success/fallback/handler value instead of defaulting to zero, returned file instant pulls can leave `STYIO_RUNTIME_FILE_OPEN_READ` on the wrapper's error channel for fallback or `io` handlers, returned stdin instant pulls can leave `STYIO_RUNTIME_NUMERIC_PARSE` for fallback or `parse` handlers, returned dynamic ranges can lower through the `__styio_list_range_i64` owned-list loop after inlining, returned list index/list-slice, dict-index, or ordered dict value-slice operations can leave `STYIO_RUNTIME_LIST_INDEX` or `STYIO_RUNTIME_DICT_KEY` for fallback or `bounds` handlers, and returned typed-parameter matrix cell/row or row-range slice operations can leave `STYIO_RUNTIME_MATRIX_INDEX` for fallback or `bounds` handlers.
17. Non-task `?| resource_operation` statements and value-required expressions lower through `SIOResourceEffect`. Codegen must emit the operation first, inspect `styio_runtime_has_error()`, clear the materialized error before running catch-all fallback or a matched named handler, guard again after recovery code, and return immediately for no-fallback settlement or unmatched named handlers without a catch-all fallback. Named handlers dispatch through `styio_runtime_error_matches_effect`; current runtime subcode families include `io`, `parse`, `bounds`, `closed`, and file-close `cleanup`, while `backpressure` must not match until a resource family emits that typed effect. Discard clears the current materialized error so the statement can continue after settlement; future diagnostics/pressure traces must not be bypassed when those channels are added. The `SIOResourceEffect::value_required` flag is the only route that may return a success/fallback/handler PHI value; while an operation is being emitted under that wrapper, file handle acquire, acquired-handle file instant pulls, checked stdin numeric conversion, typed-list stdin reads, and materialized list/dict/matrix index or row reads plus list-slice reads must leave open, closed, parse, or bounds failures on the runtime error channel for wrapper dispatch instead of returning early. Statement-shaped resource effects must continue to return the neutral statement value so ignored fallback expressions cannot become `main` exit codes.
18. Async scheduler profiling must stay opt-in: disabled runs should avoid per-task counter writes, enabled runs should expose spawn/enqueue/start/complete/pull/release and queue-depth counters through `--profile-frontend`, and task readiness should use the scheduler's low-overhead atomic wait path instead of per-task condition variables. Resource backpressure is not automatically failure: runtimes may expose `ResourceBackpressure` pressure observations for explicit side-effect logic, while only escalated pressure states such as timeout, closed channel, failed transport, or exceeded backlog become `ResourceBackpressureFailure`. `?| op | ...` discards business recovery only; runtime/codegen must still perform resource settlement, state updates, diagnostics or counters, cleanup, and commit barriers. The current pressure-observer slice stops before runtime/codegen: `resource.pressure >> #(p) => { ... }` reaches Sema and fails closed for every current resource family with `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`; do not add observer scheduling, payload layout, or runtime pressure streams without separate IM-D4/IM-D5 evidence.
19. Function and match-branch result semantics come from explicit `SGReturn` nodes emitted by lowering for final expression tails. If a lowered function body has no terminator, LLVM codegen must return the runtime default value rather than reusing the last emitted temporary from a statement tail. `SGMatch` expression result codegen must preserve `f64`, `bool`, `char`, `string`, and `i64` merge families: emit `i1` PHIs for `bool`, `i8` PHIs for `char`, `double` PHIs for `f64`, pointer PHIs for `string`, and do not widen `bool` or `char` to `i64` unless Sema intentionally merged a mixed scalar family. Tuple return annotations and unsupported container match results must be rejected before codegen until tuple/container value IR exists; do not reinterpret tuple return metadata as an `i64` function result. The tuple return rejection reports `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN` on public JSONL paths.
20. File-resource destroy lowering must use explicit handle release IR. `SIOHandleRelease` should call `styio_file_close`, clear named handle slots to zero so scope-exit cleanup is idempotent, and preserve `@("path").close()` direct-path behavior without bypassing runtime diagnostics.
21. File-resource flex rebinding must release any tracked prior file handle before overwriting the owner, then guard the cleanup boundary. Cleanup failure on default file flex-rebind must stop before the RHS acquire/overwrite and before following statements; under explicit `SIOResourceEffect` file rebind (`?| name = @file(...) | fallback`), prior-handle cleanup failure must stay on the wrapper channel before replacement open, while replacement open failures may recover through fallback or matched `io` handlers. Source-level fallback recovery for implicit cleanup and non-file reassignment cleanup remains a separate resource-effect checkpoint. Reacquiring a literal singleton path must reopen the stored slot when a prior explicit close left it at zero, while ordinary same-path aliases remain a shared singleton handle and cleanup deduplicates by slot. File handle storage must use entry-block allocas so same-path singleton reuse from later loop branches cannot reference an alloca that is dominated only by an earlier loop body. If a stale alias reads after another alias closes the shared slot, zero-handle file helpers must report `STYIO_RUNTIME_INVALID_FILE_HANDLE` and line-iterator lowering must fail fast instead of treating the null read as EOF. Function-body codegen must save, clear, and restore file/dynamic resource scope stacks around emitted function bodies so function-local cleanup state cannot leak into later functions or `main`.
22. Typed stdin pulls lower from `InstantPull` to `SIOStdStreamPull::result_type` for scalar i64/f64/string and typed list pulls. Untyped collect-bind stdin may still use `SIOListReadStdin`. String pulls must clone the borrowed stdin buffer before binding, and `list[f64]` stdin pulls must use the f64 list reader rather than falling through to i64 parsing. Numeric stdin conversion and typed-list stdin reads outside `SIOResourceEffect` must keep the default runtime guard so invalid input fails fast; inside value-required `?| (<- @stdin) | ...`, the wrapper owns `parse` fallback/handler dispatch. String concatenation should route non-string operands through `promote_to_cstr` so f64 formatting uses the same runtime decimal helper as other output paths.
23. Dynamic-slot stores must fail closed on mismatched LLVM value families. Do not replace invalid integer, floating, or pointer fields with zero/null sentinels unless the IR node explicitly represents an undefined value.
24. Internal IR operator dispatch must fail closed. Unknown binary or logical operators are typed diagnostics, not zero/left-operand fallbacks, and each new operator family needs a focused security/codegen regression before it can reach LLVM emission.
25. Runtime helpers may include public service configuration headers from `src/StyioServices/StyioConfig/`, but service-layer moves must not change the emitted helper ABI, exported `styio_*` symbol set, or ORC registration requirements.
26. Native `@extern` codegen must keep JIT and `styio build` artifacts equivalent. Inline bodies and referenced C/C++ source files both flow through `StyioNative::source_text_for_block(...)`; artifact builds must compile those units into objects and link them into the final executable so exported symbols are available without relying on process-local JIT state. When `SGExternBlock` carries explicit binding symbols from `# name[, other] := @ extern(...) { ... }`, codegen must pass that block-local symbol list instead of the legacy module-wide `@export` list.
27. Codegen must treat the independent StyioIR verifier as its input gate. `SGMainEntry`, `SGEntry`, and `SGBlock` emission must require verified active IR before LLVM builder work begins; optimization remains a separate lowering-side pass, and codegen must not reinterpret inactive, tombstone, or placeholder IR nodes as executable defaults.
27. Scalar type lowering must keep AST, Sema, IR, and LLVM type widths consistent. `char` is an 8-bit scalar through `CharAST`, `SGConstChar`, `SGType(char)`, list helpers, bounded-ring storage, and LLVM `i8`; print/output paths should route scalar chars through `styio_char_cstr` instead of integer formatting. Do not widen or default it to `i64` without a language-level scalar-width decision.
28. `SGCast` must emit real LLVM scalar conversions for compiler-owned numeric promotions. It must not return placeholder zero/default values, `bool -> int` must widen as `0` or `1`, and unsupported cast families must fail closed.
29. Stream zip over materialized non-file `list[T]` handles must use runtime `styio_list_len` / `styio_list_get_*` loops, carry the left/right element type through `SIOStreamZip`, preserve scalar widths including `char` via `styio_list_get_char`, free owned string/list/dict/matrix element temporaries after each frame, and release temporary list sources such as list literals after the finite zip exits.
30. Mixed `@file` / materialized-list stream zip must read at most one file line and one list element per frame, terminate at the shorter file EOF or list length, close the file handle on exit, and release owned temporary list sources. Do not generalize this slice to arbitrary stream drivers, topology resources, pressure effects, or timeout behavior without separate IM-D5 evidence.
31. `@stdin` stream zip is currently the accepted standard-input line-stream slice for finite zip with materialized lists or `@file` streams. It must call `styio_stdin_read_line` at most once per matched frame, bind the stdin element as `string`, terminate at stdin EOF or the shorter finite peer, and must not close stdin on exit. Duplicate `@stdin & @stdin` stays rejected until a driver-level duplicate-consumption decision lands.
32. Bounded selector snapshots in zip or iterator tails are not a new stream driver. They must arrive at codegen as materialized `SCListLiteral` / list-handle values produced by selector lowering, then use the existing runtime-list zip or `SGForEach` loop and ownership cleanup. Keep the constant-i64 `SGForEach` fast path limited to true `SGConstInt` literals so selector-history `SGResId` entries use the runtime-list path, preserve owned string/list/dict/matrix element temporaries, and do not reinterpret raw topology resources, scalar selectors, or true snapshot joins as this materialized-list slice.
33. Expression-bound range literals materialize as owned `list[i64]` handles through the internal `__styio_list_range_i64` call. Codegen must expand that helper locally with `styio_list_new_i64` / `styio_list_push_i64`, track the result for dynamic-slot and temporary-list cleanup, preserve constant-range `SCListLiteral` behavior, and keep non-integer operands rejected by Sema rather than coercing them in LLVM.
34. Native interop diagnostic refinements are diagnostic-only. `STYIO_NATIVE_SOURCE_READ_FAILED`, `STYIO_NATIVE_SIGNATURE_NOT_FOUND`, and `STYIO_NATIVE_UNSUPPORTED_SIGNATURE` classify existing fail-closed referenced-source, explicit-binding/signature, and unsupported-signature routes; do not treat those codes as new ABI support, signature support, hidden-symbol visibility, host compiler fallback, artifact linking, C++ symbol mapping, or native effect behavior.

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
5. `spio` / `view` consumers must review published runtime-event family additions or payload-shape changes.

## Handoff / Recovery

Record unfinished codegen/runtime work with:

1. IR node or runtime helper involved.
2. LLVM IR before/after expectation.
3. Runtime symbol or handle lifecycle state.
4. Failing security, pipeline, soak, or benchmark command.
5. Known rollback point and whether generated goldens are intentionally stale.
6. Runtime-event family changes and the exact consumer docs/gates updated with them.
