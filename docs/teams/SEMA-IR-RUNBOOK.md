# Sema / IR Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of AST lifecycle, semantic analysis, type inference, StyioIR lowering, string representation, and compilation session ownership.

**Last updated:** 2026-05-26

## Mission

Own the compiler middle layer from parsed AST to StyioIR and stable textual representation. This team protects AST ownership, type contracts, lowering shape, and reprs used by diagnostics and five-layer goldens. It does not own parser syntax or LLVM emission.

## Owned Surface

Primary paths:

1. `src/StyioAST/`
2. `src/StyioSema/`
3. `src/StyioLowering/`
4. `src/StyioIR/`
5. `src/StyioToString/`
6. `src/StyioSession/`
7. `src/StyioResourceTopology/`
8. `src/cmake/StyioFrontendSources.cmake`

High-value docs:

1. [../design/Styio-Language-Design.md](../design/Styio-Language-Design.md)
2. [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md)
3. [../../workflows/FIVE-LAYER-PIPELINE.md](../../workflows/FIVE-LAYER-PIPELINE.md)
4. [../design/Styio-Resource-Topology.md](../design/Styio-Resource-Topology.md)

## Daily Workflow

1. Start from the language or capability SSOT for the feature.
2. Identify the AST node, type-inference rule, lowering rule, and IR node together before editing.
3. Keep `StyioSemaContext` responsible for type inference/state and `AstToStyioIRLowerer` responsible for AST-to-IR conversion. The historical `StyioAnalyzer` compatibility alias has been removed; new code references the canonical class names directly.
4. Keep ownership/view changes small and covered by safety or security tests.
5. Update five-layer goldens when AST or StyioIR textual shape intentionally changes.
6. Coordinate with Codegen / Runtime before changing IR consumed by LLVM emission.
7. Keep semantic lowering fail-closed: unknown user calls, user-call arity mismatches, unsupported AST nodes, and missing type slots must produce typed diagnostics or covered lowering rules, not `SGConstInt(0)` placeholders.
8. When adding or repairing AST nodes such as `SizeOf`, prove the full lifecycle: owned child expression, writable inferred type slot, typed inference result, and StyioIR lowering shape.
9. When parser syntax can represent one-shot continuations before lowering exists, emit explicit semantic errors instead of letting internal resume names leak as unknown user functions.
10. For terminal-handle and standard-stream iterable writes (`>> [>_]`, `>> @stdout`, `>> @stderr`), keep semantic checks stricter than `->`: require an iterable text-serializable type, reject scalar strings, and route explicit `string.lines()` through `list[string]` lowering.
11. Keep GenIR domain ownership physical as well as nominal: default/general nodes belong in `src/StyioIR/GenIR/SGIR.hpp`, IO/file/std-stream/network/filesystem nodes in `SIOIR.hpp`, and List/Dictionary/Matrix collection nodes in `SCIR.hpp`. `GenIR.hpp` is a compatibility aggregator, not the primary place for new nodes.
12. Normalize break depth at the AST and IR boundary. `BreakAST` and `SGBreak` may keep compatibility constructors, but their semantic depth is always 1 and lowering must not preserve historical multi-level counts.
13. Native `@extern` declarations must have a real middle-layer lifecycle: parse only top-level exported signatures, expose callable names to type inference, lower `@export` and `@extern` to explicit SG nodes, and reject unknown native calls before codegen. Do not inspect or reinterpret native function bodies.
14. `InfiniteLoopAST` must type-check its guard and body before lowering; conditional loop guards are bool-only, so non-bool values must fail in sema rather than reaching LLVM codegen.
15. Matrix typed literals and intrinsics must carry element kind and static shape through Sema into IR. Reject ragged rows, nonnumeric elements, add/sub shape mismatches, and invalid matmul dimensions before lowering; lower `m[row]`, `m[row][col]`, arithmetic operators, and `mat_*` intrinsics to explicit collection IR instead of placeholder constants.
16. Match lowering must emit ordinary `SGMatch` shape and leave sequence-aware equivalence rewrites to `StyioIROptimizer`; do not hard-code source examples such as `.length` / `.size` in AST lowering. Syntax aliases are not equivalent until StyioIR structure, side-effect safety, and tests prove it.
17. Runtime resource bindings must keep value-family identity through Sema and lowering. Matrix handles are dynamic slot values just like list and dict handles; name loads must lower through the matching `SGDynLoadKind` instead of reusing stale SSA handles.
18. Task resource bindings follow the same value-family rule: `TaskBlockAST` must infer `task[T]`. Ordinary `FlowBindAST` still requires a predeclared mutable target, while `?| job -> answer: T | fallback` declares the await target and consumes the task/future handle once before lowering to `SIOTaskCreate` plus `SIOFlowBind`. Without fallback, `?| job -> answer: T` and `?| resource_operation` are immediate settlement points that raise structured errors in place rather than carrying errors forward. With fallback, `?| resource_operation | fallback` must infer one recovered value type from the success path, fallback path, and surrounding use site. `?| resource_operation | effect => handler` handles only that typed effect family and must also infer a value compatible with the operation success type; chained handlers must reject duplicate effects and keep any catch-all fallback last. `?| op | ...` is a standalone statement-only resource-effect discard: Sema must reject it anywhere a value is required, must not synthesize a success value, and must still settle resource state, diagnostics, pressure accounting, cleanup, and commit effects. `?| op | effect => @()` remains invalid because `@()` is not an executable empty action. `?=` must not be treated as a resource-effect handler; it may only match an explicitly materialized ordinary result value. Bare `?| -> answer: T` must fail closed until continuation lowering can guarantee one-shot resume/discontinue. Free scalar references inside `||>` are captured into the task context; local binds inside the task body must not inflate that context.
19. The current non-task resource-effect implementation has statement and first value-producing expression slices: Sema accepts `ResourceEffectAST` for `?| resource_operation`, catch-all `?| resource_operation | fallback`, named handlers / handler chains over the known effect names `io`, `parse`, `bounds`, `closed`, `backpressure`, and `cleanup`, and discard `?| resource_operation | ...`; lowering emits `SIOResourceEffect`. Resource topology must visit the wrapped operation, fallback, and handlers instead of treating `ResourceEffectAST` as an opaque value, so statement-shaped file handle acquire records its handle binding and later file iterator or `?| f.close() | fallback` close-method paths can use the acquired handle. A later close still consumes the receiver for topology use-after-destroy checks. Statement-shaped file handle acquire is accepted only for file resources such as `@file(...)` / `@{...}` and remains rejected in value-required `?|` expressions; a recovered failed acquire followed by later iteration must still fail closed through the zero-handle runtime diagnostic. Statement-shaped resource method calls are accepted only when the receiver resolves to a known resource family and the member is a callable resource method; ordinary member calls such as `text.lines()` must fail closed under `?|`. Expression parsing marks value-required resource effects, and Sema must reject that form unless the operation produces a value. The current executable value paths are file instant-pull `i64` success/fallback/handler recovery, untyped stdin numeric instant-pull `i64` parse fallback/handler recovery, explicit-target stdin `f64`, `string`, and supported typed-list value recovery through the same `InstantPullAST` result type, materialized `list[T]` single-index and slice recovery through `STYIO_RUNTIME_LIST_INDEX` / `bounds`, materialized dict single-key recovery through `STYIO_RUNTIME_DICT_KEY` / `bounds`, and materialized matrix cell/row recovery through `STYIO_RUNTIME_MATRIX_INDEX` / `bounds`. Reject unknown or duplicate handler names, reject `effect => @()`, keep a final catch-all fallback last, reject unsupported typed stdin targets such as `bool`, and keep dict/matrix slice-shaped resource-effect recovery fail-closed until those operation families have their own tests. Do not treat these expression slices, statement file-acquire recovery plus later file iteration or close-method use, or statement file-close method settlement as proof that arbitrary value-producing resource-effect expressions, broader post-acquire operations, implicit/reassignment cleanup effects, non-file cleanup families, or pressure-observer semantics are complete. Runtime cleanup matching is currently limited to explicit file close failure from the file-write resource path.
20. Resource pressure observers such as `channel.pressure >> #(p) => { ... }` are side-effecting resource paths, not fallback syntax. Sema must require a resource family that explicitly exposes a pressure stream, type the pressure payload fields such as pending count and high-water mark, and keep observer body effects on the same snapshot/commit and `?| ... | fallback` rules as ordinary resource code. Backpressure is `ResourceBackpressure` until an operation-family policy escalates it to `ResourceBackpressureFailure`.
21. Match expression result kinds must preserve scalar families through Sema and IR. `MatchCasesAST` must type-infer the scrutinee, accepted integer case patterns, arm/default bodies, and branch-local scopes before lowering; function match sugar must run the same body inference at call sites with a recursion guard so recursive base arms can establish the return family. If tail expressions can yield `f64`, the `SGMatchReprKind` and lowering classifier must carry a float result kind instead of silently collapsing the branch value to `i64`. Undefined arm tail values and branch-local binding leakage must fail in Sema before `SGMatch` reaches codegen.
22. Resource topology graph validation is part of the Sema-to-Lowering boundary. Changes to file resources, standard streams, handles, state slots, hidden ledgers, stream ops, or task resources must update `src/StyioResourceTopology/` before lowering can accept the new shape.
23. Retired state AST nodes may remain as internal ledger/lowering structures and ownership-test fixtures, but source syntax must enter through Topology v2 resources. User-facing diagnostics should point to `@name : Type`, `expr -> @name`, and `@name[-1]`, not to the old state-resource spelling.
23. Resource method semantics must resolve statically before lowering: unknown methods are compile errors, consuming methods such as close/drop/destroy invalidate the receiver immediately, and transitive calls from one receiver method to another consuming method must inherit consuming status. `resource -> @()` is the intrinsic destroy sink, scope exit adds automatic drop edges for close-capable owned resources, and task bodies may borrow outer resources but must not consume them. Lowering must consult the resolved method table's consuming flag so user overrides are not treated as destroy operations by name alone. Unordered named task or block bodies that take exclusive access to the same resource must be rejected unless an explicit `=>` happens-before edge orders them.
24. A successful flex resource rebind is a new occupant for that source name. Sema may clear the destroyed-receiver mark only after the RHS resource construct or clone has type-checked; use-after-destroy without a rebind must remain a stable error, and scalar rebinding after close remains outside the accepted resource-rebind slice until value-family storage rules cover it.
24. `InstantPullAST` carries the result type for typed stdin pulls. Keep scalar and typed `list[T]` stdin pulls on the same AST and `SIOStdStreamPull` path, reject unsupported stdin list element families in sema, and infer `ReturnAST` expressions before deriving `task[T]` so f64 task bodies do not collapse to i64 handles.
25. Built-in method names such as list `push/insert/pop`, string `lines`, and resource `write/close/drop/destroy` must be classified through `StyioUtil/BuiltinMethods.hpp`; sema, lowering, and topology must not keep independent string lists.
26. Format strings lower through ordinary string concatenation: infer each embedded expression, report the result as `string`, and reuse existing string/numeric runtime conversion rather than inventing a separate formatting IR node. Undefined hash-tag iterator sequences must stay fail-closed until the design SSOT defines their semantics.
27. Internal lowering dispatch must reject unknown comparison, list, and logical operator values with `StyioTypeError`. Do not map unknown enum values to equality, constant zero, raw value, or other placeholder IR.
28. IR and lowering ownership must be exception-safe across optimizer rewrites. When a lowering path creates temporary AST or IR nodes, keep a local owner until the target IR node adopts them; when an optimizer replaces or hoists child IR, either transfer that exact pointer into the new owner or delete the superseded child before overwriting the field. ASan security coverage is required for parser recovery seeds and IR rewrite paths that previously leaked.
29. Native `@extern` source references are middle-layer metadata, not parsed Styio syntax. Preserve `ExternBlockAST::getSourcePaths()` and explicit binding symbols from `# name[, other] := @ extern(...) { ... }` through `SGExternBlock`, include them in textual reprs for diagnostics/goldens, and let native signature discovery read the referenced C/C++ sources during sema so missing files fail as typed semantic errors rather than parser errors. Bound externs must register only the named symbols and fail if a named native function is not declared by that block.
30. Every concrete `StyioIR` node must expose `is_active()`. Current canonical IR nodes default to active through `StyioIRTraits`; any future retired, tombstone, compatibility-only, or placeholder IR node must override it to `false`. The independent `StyioIR` verifier owns contract state such as resource/handle capability side tables, runs after lowering and before codegen, and rejects inactive nodes on accepted execution paths.
31. Intentional empty statement forms lower to explicit `SGNoOp`, not scalar sentinel values. `CommentAST`, `EmptyAST`, and `PassAST` are no-op statements; expression-like absence such as `NoneAST` must not be silently reclassified as no-op without a separate language decision.
32. Keep [../rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md](../rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md) aligned with lowering reality. Accepted metadata such as resource preludes or resource method definitions may lower to `SGNoOp`; accepted executable sugar such as function-level match cases needs real StyioIR; unsupported value syntax must fail closed instead of returning `SGConstInt(0)`.
33. `TypeConvertAST` is accepted only for compiler-owned scalar promotions in this slice. It must carry its source value into `SGCast(value, from_type, to_type)`, remain visible to verifier and optimizer traversal, and stay separate from source-level cast syntax until an explicit language decision accepts that syntax.
34. Topology v2 resource selectors must keep distinct value families through Sema and lowering. `@name[-n]` is a scalar/latest history read, while bounded `i64`/`f64`/`bool`/`char`/`string` `@name[-n..]` and `@name[...]` lower to typed list materialization from explicit history reads. Do not collapse slice or snapshot selectors back to whole-resource `SGResId`; unsupported non-bounded, unsupported value-family, or out-of-window selectors must fail closed.
35. Explicit selector copy builds on the same value-family rule: `snapshot << @name[-n..]` and `snapshot << @name[...]` bind the materialized list selector value for bounded `i64`/`f64`/`bool`/`char`/`string` resources. Do not route these forms through resource-write lowering, and keep scalar/latest `@name[-1]` out of the snapshot-copy path.
36. Stream zip element families must survive from Sema through lowering. When a zip input is a materialized `list[T]` handle rather than a literal or file source, bind the closure parameter as `T` and carry `T` into `SIOStreamZip`; codegen must not infer every non-string zip element as `i64`.
37. Mixed file/list zip source-shape facts must stay explicit in Sema and IR: `@file` contributes the file element family, the opposite side must be a materialized `list[T]` handle for the accepted slice, and scalar or broader resource inputs must fail closed before codegen can reinterpret them as list handles.
38. Bounded Topology selector snapshots may feed stream zip only after the selector has inferred and lowered to a materialized `list[T]` value. Keep `@name[-n..]` and `@name[...]` on the same value-family path as other materialized lists, and keep scalar/latest `@name[-1]` rejected as a non-iterable zip input.
39. Range literals are accepted only for integer `start`, `end`, and optional `step` expressions in this slice. Sema should infer expression-bound range values as materialized `list[i64]` handles for binding, printing, and stream/list consumers; lowering may still use the dedicated `SGRangeFor` path for iterator bodies. Non-integer bounds must fail before codegen.
40. Function return type metadata must not become placeholder runtime types. Scalar and supported container return annotations may lower normally, unspecified return types may infer from accepted tail expressions, but tuple return annotations must fail closed until tuple value IR exists instead of falling back to `i64`.
41. Materialized `list`, `dict`, and `matrix` handle clones use the explicit `name << source` surface. Sema must reject `name <- source` for already-bound cloneable resources because `<-` is resource-entry acquire/receive or task/future pull, while `<<` lowers through `SCListClone` / `SCDictClone` / `SCMatrixClone` and preserves independent owned containers.
42. State/resource-method inline cloning must cover accepted source-reachable value ASTs before those forms are considered executable inside helper bodies. `StateExprCloneVisitor` currently clones `CharAST` along with the existing scalar leaves, so `@file::marker = () => { >_('x') }` can inline and run; keep the fallback diagnostic for unsupported node families until each accepted helper-body form has parser, clone, lowering, and runtime evidence.

## Change Classes

1. Small: local type rule, repr text fix, or non-contract helper cleanup. Run targeted unit and affected feature tests.
2. Medium: AST node field, ownership, type inference, or lowering change. Add security or pipeline coverage and update goldens.
3. High: new semantic category, IR node family, session lifecycle rule, or capability/failure model change. Use checkpoint workflow and add ADR if lifecycle or compatibility changes.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build/default -L language_feature
ctest --test-dir build/default -L styio_pipeline
ctest --test-dir build/default -L security
ctest --test-dir build/default -L resource_topology
```

When AST or IR text changes:

```bash
STYIO_PIPELINE_DUMP_FULL=1 ctest --test-dir build/default -L styio_pipeline --output-on-failure
```

For checkpoint-grade validation:

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

## Cross-Team Dependencies

1. Frontend must review changes that require new AST construction or parser recovery behavior.
2. Codegen / Runtime must review IR shape or type changes consumed by LLVM lowering.
3. Test Quality must review five-layer, semantic failure, and security coverage.
4. Docs / Ecosystem must review capability or design SSOT updates.

## Handoff / Recovery

Record unfinished middle-layer work with:

1. AST nodes and ownership state.
2. Type-inference and lowering rule status.
3. Expected repr or IR text delta.
4. Failing five-layer layer, if any.
5. Whether Codegen has already been adapted.
6. Remaining unsupported-lowering handlers and the negative matrix needed to retire placeholder fallback safely.
