# Current Code vs Design Gap Audit 2026-05-24

**Purpose:** Record the current implementation-to-design gap audit for the `styio` repository after the recovery-era rebuild, using the live code, docs, build tree, and local gates as evidence.

**Last updated:** 2026-05-31

**Status:** Active rollup. Use this as a current, evidence-backed audit companion to
[`CURRENT-STATE.md`](./CURRENT-STATE.md) and [`NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md).

## Scope

This audit compares the current checkout against the active design and contract
documents in `docs/design/`, `docs/specs/`, `docs/external/`, `docs/rollups/`,
`workflows/`, and the actual implementation under `src/`, `tests/`, `benchmark/`,
and `example/`.

The key question is not whether old lost code can be reconstructed from memory.
The key question is which design promises have a real compiler/runtime/test path
today, which promises are target design only, and which missing surfaces are
intentionally owned by adjacent repositories.

## Verification Snapshot

Current local facts:

| Area | Result |
|------|--------|
| Branch/worktree | `nightly...origin/nightly`, no tracked dirty files before writing this audit |
| Compiler binary | `build/bin/styio --version` prints `styio 0.0.1` |
| Machine contract | `build/bin/styio --machine-info=json` reports `channel=nightly`, `active_integration_phase=compile-plan-live`, and contracts for `machine_info`, `jsonl_diagnostics`, `syntax_check`, `compile_plan`, and `runtime_events` |
| Docs/parser smoke | `docs_audit`, README glimpse example, syntax-check parser-authority test, and parser legacy-entry audit passed |
| Language feature suite | `ctest --test-dir build -L language_feature --output-on-failure`: 126/126 passed |
| Security/safety suite | `ctest --test-dir build -L security --output-on-failure`: 258/258 passed |
| Pipeline suite | `ctest --test-dir build -L styio_pipeline --output-on-failure`: 167/167 passed |
| Parser shadow gates | `ctest --test-dir build -R '^(parser_shadow_gate_|parser_legacy_entry_audit)' --output-on-failure`: 6/6 passed |
| Algorithm equivalence | After building the missing target, `ctest --test-dir build -L algorithm_equivalence --output-on-failure`: 36/36 passed |
| Performance smoke | After building the missing target, `ctest --test-dir build -L performance --output-on-failure`: 2/2 passed |
| Deep soak | After building the missing target, `ctest --test-dir build -R '^(styio_algorithm_equivalence_test_NOT_BUILT\|soak_deep_)' --output-on-failure`: 9/9 soak tests passed; the prior `NOT_BUILT` placeholder disappeared after target discovery |

Important nuance: before explicitly building `styio_algorithm_equivalence_test`,
`styio_soak_test`, and `styio_task_scheduler_perf_test`, CTest had registered
placeholder or target-backed tests that were `Not Run` because the executables
were absent from `build/bin`. That was a local build-state/target-selection
problem, not evidence that the source targets are missing.

## What Is Real

The current repository is a working recovered baseline, not an empty shell.

Real compiler/runtime surfaces:

1. A runnable compiler CLI exists and handles single-file execution, AST/IR/LLVM
   dumps, JSONL diagnostics, source-build metadata, compile-plan input, and nano
   package materialization/publish/consume paths.
2. The nightly parser is the default accepted parser. Parser shadow gates for
   scalar expressions, functions, file resources, and stream-processing fixtures
   pass with zero accepted-grammar fallback in the checked suites.
3. The language feature matrix has runnable coverage for scalar expressions,
   functions, control flow, wave dispatch, resource topology resource slots, final
   bindings, native interop, tasks, stdio, file resources, and selected stream
   processing.
4. Direct unsupported AST lowering now fails closed or lowers intentional empty
   forms to `SGNoOp`; codegen verifier gating and security tests are present.
5. Accepted single-quoted `char` literals, format strings, dynamic range
   literals, match expressions, ordinary function-call returns including
   explicit `matrix` return functions, value-producing resource-effect expressions, and single-return list/dict/matrix container bounds expressions are
   executable in ordinary expressions and in user-defined resource method bodies
   after inlining. The resource-method body parser now admits the same
   `CharAST`, `FmtStrAST`, `RangeAST`, `MatchCasesAST`, value-producing `ResourceEffectAST`,
   `FuncCallAST`, list index/slice, inline `dict{...}` index, and typed-parameter matrix
   cell/row or row-range slice value shapes, and `StateExprCloneVisitor` clones
   them instead of reporting an unsupported inlined state expression.
6. `@extern(c|c++)` is not just design prose: native interop feature tests and
   native executable build tests pass.
7. The IDE/LSP core exists for completion, hover, definition, references,
   document/workspace symbols, semantic tokens, diagnostics, incremental sync,
   and request-driven semantic drain.
8. Nano package and `compile_plan` integration are real compiler-side contracts;
   full package-manager UX is explicitly outside this checkout.

## Highest-Priority Gaps

### P0. Resource-effect `?|` has fallback settlement, but not the full unified model

Design says `?| resource_operation`, `?| resource_operation | fallback`,
effect-specific handlers, and audited discard are the uniform resource-effect
surface. See `docs/design/syntax/ACTIVE-SYNTAX.md` resource rows for fallback,
handler, discard, and pressure observer, and `docs/design/Styio-Language-Design.md`
task/resource-effect section.

Current implementation reality:

1. The parser entry for `?|` now routes through
   `parse_await_or_resource_effect_stmt_nightly` in
   `src/StyioParser/NewParserExpr.cpp`; it preserves typed task_await binding
   for `?| task -> name: T` and routes non-typed resource operations to a
   resource-effect statement path.
2. Resource-effect statements now lower through an explicit resource-effect
   AST/IR wrapper. `?| resource_operation` settles the operation and emits a
   runtime-error guard at that source site, statement-only
   `?| resource_operation | ...` settles and discards business recovery, and the
   catch-all fallback slice accepts `?| resource_operation | fallback` for
   statement-shaped resource operations. The first named-handler slice accepts
   statement-shaped `?| resource_operation | effect => handler` and handler
   chains, stores them through AST/IR, rejects duplicate or unknown handler names
   in Sema, and dispatches matched runtime subcodes through the current resource
   error channel.
3. Task/future pull settlement now has parser/Sema/lowering/runtime evidence:
   `?| task -> name: T` and `?| task -> name: T | fallback` lower through
   `SIOFlowBind`; failed task pulls run the await fallback only after clearing
   the materialized task error; failed task pulls without fallback stop at the
   await settlement site; non-task await sources and bare continuation freeze
   fallbacks fail closed.
4. File-backed resource-write and handle-acquire smoke coverage proves fallback
   and named `io` handlers run only after the resource failure is materialized
   and cleared, successful operations skip recovery, unmatched handlers such as
   `backpressure` for a file-open failure fall through to the default fail-fast
   rule when no catch-all fallback is present, handler chains continue to a final
   catch-all fallback when no named handler matches, and
   `?| resource_operation` without fallback stops before the next statement.
   The accepted statement-shaped acquire slice covers
   `?| f <- @file(missing) | fallback` and matched `io` handlers for file-open
   read failures, and the successful acquire path now records the file handle in
   resource topology so a following `f >> #(line) => { ... }` iterator can use
   it. The same acquired handle can feed a later resource-effect close method,
   later guarded `f.write(...)` write method, or later value-producing
   `?| (<< f) | fallback` instant-pull expression, and the close still consumes
   the receiver in topology so following `f.path` use fails closed. If the
   acquire failure is recovered and the zeroed handle is later used, the file
   iterator, guarded write method, and acquired-handle instant pull report
   `STYIO_RUNTIME_INVALID_FILE_HANDLE` instead of treating the handle as a valid
   stream or recreating the file by path. Direct file iterator settlement is
   also covered as a statement-shaped resource operation:
   `?| @file(missing) >> #(line) => { ... } | fallback` and matched `io`
   handlers recover `STYIO_RUNTIME_FILE_OPEN_READ`, successful file iteration
   skips recovery, no-fallback settlement fails fast before the following
   statement, and non-file iterators such as list iteration stay rejected under
   `?|`.
5. File-close cleanup failure now has a real runtime subcode family:
   `fclose` failure is reported as `STYIO_RUNTIME_FILE_CLEANUP_FAILURE`,
   `styio_runtime_error_matches_effect("cleanup")` matches it, source-level
   `?| "x" -> @file("/dev/full") | cleanup => handler` recovers through the
   named cleanup handler, and adjacent `io => handler` does not catch it.
6. Plain file resource-operation statements outside a `?|` wrapper now settle at
   the statement boundary instead of carrying a runtime error into later
   statements. `f <- @file(missing)`, `"x" >> @file(missing-dir/out)`, and
   direct `@file(missing) >> #(line)` iteration emit the JSONL runtime
   diagnostic and stop before a following `>_("after")`, while the same
   operation-local guard is suppressed inside `SIOResourceEffect` so catch-all
   fallback and named handlers still recover, including statement-shaped
   `?| f <- @file(missing) | fallback` acquire recovery and direct
   `?| @file(missing) >> #(line) => { ... } | fallback` iterator recovery.
7. File-resource flex rebinding now covers the source-reachable cleanup edge for
   `name = @file(...)`: Sema treats a successful resource rebind as a new
   occupant after a consuming `.close()`, Codegen releases the prior tracked file
   handle before the RHS acquire/overwrite, guards the cleanup error channel
   before opening the replacement resource or running following statements, and
   same-path singleton slots left at zero by an explicit close are reopened
   before later iteration. Statement-shaped `?| name = @file(...) | fallback`
   is now accepted only for file-resource flex rebinds: replacement open
   failures recover through catch-all fallback or matched `io` handlers, scalar
   flex binds stay fail-closed, value-required rebind expressions stay rejected,
   and the cleanup-error branch reaches `SIOResourceEffect` before any
   replacement open can run.
8. File iterator error/EOF separation now covers same-path alias invalidation:
   zero file handles diagnose as `STYIO_RUNTIME_INVALID_FILE_HANDLE`, and
   `SIOFileLineIter` checks the runtime error channel before treating a null
   line as normal EOF. `f1.close(); f2 >> #(line)` over a shared same-path file
   slot fails fast instead of silently continuing after the iterator.
9. File scope-exit cleanup now covers tracked file handles on ordinary
   scope-pop cleanup, explicit `<| return`, and loop control-flow exits.
   `SGReturn` emits active file-handle cleanup before the LLVM `ret`, loop `^`
   break and standalone `>>` continue branches clean the resource scopes they
   bypass before jumping to the loop target, normal scope-pop cleanup checks the
   runtime error channel after cleanup, and function-body codegen saves/restores
   resource scope state so function-local cleanup stacks do not leak into later
   codegen. File handle slots are entry allocated so same-path singleton reuse
   across loop branches does not violate LLVM dominance. This is a tracked-file
   cleanup settlement slice; source-level fallback recovery for implicit cleanup,
   non-file reassignment cleanup, and non-file cleanup families remains open.
10. Statement-shaped resource method calls now participate in resource-effect
   settlement. `?| @file("data.txt").close() | fallback` skips recovery after a
   successful open/close, missing direct file close recovers through catch-all
   fallback or a matched `io` handler after clearing the materialized
   `STYIO_RUNTIME_FILE_OPEN_READ`, and no-fallback settlement stops before the
   following statement. Sema rejects non-resource member calls such as
   `text.lines()` under `?|` so ordinary method calls do not become implicit
   resource effects.
   Explicit direct file release through `?| @file("data.txt") -> @() | fallback`
   now has the same statement settlement evidence: success skips recovery,
   missing-file open failures recover through catch-all fallback or matched `io`,
   no-fallback settlement stops before the following statement, and
   value-required release expressions remain rejected.
11. The first value-producing non-task resource-effect slices are executable for
   file and stdin instant pulls, materialized container bounds reads, and simple
   value-returning resource methods:
   `result = ?| (<< @file("data.txt")) | fallback`
   returns the successful `i64` file line value on success, clears a materialized
   file-open read failure before evaluating the fallback on failure, supports
   named handler value branches such as `io => 9`, and fails fast without a
   fallback. After a successful `?| f <- @file("data.txt") | ...`,
   `result = ?| (<< f) | fallback` returns the next `i64` from that acquired
   handle; if a recovered failed acquire leaves the slot at zero, matched
   `closed => value` handlers recover `STYIO_RUNTIME_INVALID_FILE_HANDLE` and
   no-fallback settlement fails before the following statement. Non-file names
   such as materialized lists remain rejected as acquired-handle instant-pull
   sources. `result = ?| (<- @stdin) | fallback` now follows the same
   value-required path for untyped stdin numeric pulls: successful stdin lines
   return the parsed `i64`, numeric parse failures recover through catch-all
   fallback or a matched `parse => handler`, and no-fallback expression
   settlement still reports `STYIO_RUNTIME_NUMERIC_PARSE` before later
   statements. Explicit target types now flow into stdin resource-effect pulls
   as well: `result: f64 = ?| (<- @stdin) | fallback` returns or recovers `f64`
   values, `result: string = ?| (<- @stdin) | fallback` returns a cloned stdin
   line, and `result: list[i64] = ?| (<- @stdin) | fallback` materializes or
   recovers typed list values while list parse failures report
   `STYIO_RUNTIME_LIST_PARSE` without fallback. `result = ?| xs[i] | fallback`,
   `result = ?| d[key] | fallback`, `values = ?| d[start..end] | fallback`,
   `result = ?| m[row][col] | fallback`,
   `row = ?| m[row] | fallback`, `rows = ?| m[start..end] | fallback`, and
   `slice = ?| xs[0..] | fallback` now also return successful materialized
   container values, recover `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`,
   or `STYIO_RUNTIME_MATRIX_INDEX` through catch-all fallback or a matched
   `bounds => handler`, and fail fast without a fallback. Ordered dict value
   slices reuse the ordered `d.values` materialization and list-slice bounds
   path, so out-of-range dict slice bounds report `STYIO_RUNTIME_LIST_INDEX`.
   Plain `xs[i]`,
   `xs[0..]`, `d[key]`, `d[start..end]`, `m[row][col]`, and `m[start..end]` expressions outside
   `?|` now guard the same runtime bounds failures before a following statement.
   User-defined resource methods with a single `<| expr` body, or an accepted
   statement-only preface followed by a final `<| expr`, now record the returned
   value type, direct calls such as `log.answer()` no longer lower an inlined
   `SGReturn` into expression context, and
   `result = ?| log.answer() | fallback` returns the successful method value
   while keeping fallback type mismatches fail-closed. Statement-preface methods
   such as `@file::answer = () => { >_("inside") <| 42 }` run the preface and
   still return the value through direct and guarded calls. Returned bool, f64, char,
   string, and format-string expressions preserve their value family through
   direct calls and guarded value paths, including
   `@file::summary = (x: int) => { <| $"value={x + 1}" }`. Returned dynamic
   range literals such as `<| [start..stop..step]` inline as ordinary `list[i64]`
   success values and keep non-integer range bounds fail-closed before lowering.
   Returned match expressions such as
   `@file::pick = (x: int) => { <| x ?= { 0 => 'a' _ => 'b' } }` preserve
   `i64`/`f64`/`bool`/`char`/`string` result families through direct calls and
   guarded value paths, while returned container match results still fail closed
   before lowering.
   Returned calls to ordinary block-form functions such as
   `# plus_one := (x: i64) => { <| x + 1 }` plus
   `@file::score = (x: i64) => { <| plus_one(x) }` now infer the called
   function's explicit return/final-tail result type for direct and guarded
   method calls; statement-only function bodies remain fail-closed as method
   return values.
   Explicit matrix return annotations now also apply matrix literal context to
   returned nested-list tails, so `# make : matrix = () => { <| [[1,2],[3,4]] }`
   can feed `result: matrix = make()`, `log.make()`, and
   `?| log.make() | [[9,9],[8,8]]`; flat-list matrix returns such as
   `<| [1,2]` fail closed in Sema before reaching runtime.
   Returned value-producing resource-effect expressions such as
   `<| ?| (<< @file("data.txt")) | io => 8 | 7` now parse through the nightly method
   body path, preserve their inferred result type during inline cloning, return
   their internal success/fallback/handler value through direct calls, and keep
   returned `?| op | ...` discard rejected as statement-only.
   When that single returned
   expression is a file instant pull, `result = ?| log.read_missing() | fallback`
   now recovers `STYIO_RUNTIME_FILE_OPEN_READ` through catch-all fallback or a
   matched `io => handler`, and no-fallback settlement stops before the
   following statement. When the returned expression is the canonical
   parenthesized stdin instant pull `(<- @stdin)`,
   `result = ?| log.read_stdin() | fallback` returns the parsed `i64` on
   success, recovers `STYIO_RUNTIME_NUMERIC_PARSE` through catch-all fallback or
   a matched `parse => handler`, and fails fast before following statements
   without a fallback. When the returned expression is a materialized list
   index, list slice, inline dict index, ordered dict value slice, or typed-parameter matrix cell/row or
   row-range slice read, `?| method() | fallback` recovers
   `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, or
   `STYIO_RUNTIME_MATRIX_INDEX` through catch-all fallback or a matched
   `bounds => handler`, and no-fallback dict-key or matrix-index settlement
   fails before the following statement. Resource method
   declared parameter types are bound while inferring the method body and checked
   at call sites, while lexical/global captures remain fail-closed before
   lowering. Scalar local `=` and `:=` prefaces plus local list/dict/matrix `=` and `:=`
   prefaces that return scalar/string values or the local list/dict/matrix container
   value now have isolated method value-scope semantics and inline-clone
   hygienic renaming; returned local list/dict/matrix handles are cloned before
   method-scope cleanup. Local resource binding prefaces intentionally
   fail closed before lowering until their value-scope semantics are implemented.
   Parser/Sema keep `?| op | ...` statement-only, reject statement-shaped write
   operations where a value is required, reject returned resource-effect discard,
   and reject fallback type mismatches.
12. Pressure observer syntax now reaches the correct fail-closed resource-family
   boundary: `channel.pressure >> #(p) => { ... }` parses through the nightly
   iterator/attribute path, `channel.pressure` on a topology resource and
   `@stdout.pressure` fail in Sema before codegen, and public JSONL diagnostics
   report `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED` with phase
   `sema`. This is closed evidence for the parser/Sema/diagnostic boundary only,
   not pressure payload typing or runtime observer execution.
13. There is still no complete typed value-producing resource-effect model for
   arbitrary resource operations beyond the covered file/stdin instant pulls,
   acquired-handle file instant pulls,
   materialized container index/row/slice reads, materialized list slices, and simple
   resource-method single-return or statement-preface/scalar-local and list/dict/matrix-local scalar/string or local-container-return bodies, no recovery model for failing
   value-producing resource methods beyond the returned file/stdin instant-pull,
   returned explicit matrix-valued function success paths, returned local
   list/dict/matrix container returns, and returned list/dict/matrix bounds failure
   slices, or for local resource binding method bodies and
   resource-method lexical/global captures, no
   source-level fallback recovery model for implicit cleanup or non-file
   reassignment cleanup, no cleanup families beyond file close, no
   resource family that emits a non-failure
   `ResourceBackpressure` pressure event, and no pressure-observer runtime
   implementation beyond the fail-closed unsupported-family boundary.

Impact: statement-shaped fallback, audited discard, and named handler chains now
have a real parser/Sema/IR/codegen/runtime path for current runtime error
subcode families such as `io`, plus the first file-close cleanup-failure family.
Task await fallback settlement for failed task pulls is now covered, and plain
file acquire/write/release failures now fail fast before subsequent statements
when no `?|` recovery wrapper is present, direct file iterators fail fast on open
failure, successful direct file release continues normally, and same-path
aliases now report closed-handle use instead of normal EOF after another alias
closes the shared slot. Explicit returns, ordinary
scope-pop exits, and loop break/continue exits now close tracked file handles
before leaving the resource scope and settle cleanup failures at that boundary.
Resource method calls such as
direct file close and statement-shaped file handle acquire now enter the
statement `?|` recovery path, including catch-all fallback, matched `io`
handlers, no-fallback fail-fast settlement, and non-resource member-call
rejection for method candidates. Direct file iterators now enter the statement
`?|` recovery path as well: success skips fallback, missing-file open failures
recover through catch-all fallback or matched `io` handlers, no-fallback
settlement fails fast, and non-file iterators remain rejected. File/stdin instant-pull, materialized
container-index, materialized list-slice, and simple resource-method
single-return resource-effect expressions now cover the first typed
success/fallback/handler value paths, including explicit-target stdin `f64`,
`string`, typed-list pulls, and `bounds` recovery for
`STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, and
`STYIO_RUNTIME_MATRIX_INDEX` under `?|`; simple resource methods that return a
file instant pull or canonical stdin instant pull also recover matched `io` or
`parse` failures under `?|`, explicit matrix-return ordinary functions now feed
direct matrix bindings, resource-method values, and matrix fallback literals
without producing invalid matrix handles, and simple resource methods that return
materialized list index/list-slice, inline dict-index, or typed-parameter matrix
row-range slice expressions recover the covered `bounds` failures under `?|`,
while direct `log.answer()` resource method calls no longer abort lowering.
Typed resource method parameters now feed method-body inference and call-site
checks, so returned matrix cell/row or row-range slice bounds failures recover
through matched `bounds` handlers or catch-all fallback under `?|` without
opening global matrix capture. Pressure observer syntax now has parser/Sema
boundary coverage and a public unsupported-family diagnostic, so current resource
families fail closed with `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`
instead of surfacing a broad parser subset or generic type error.
Source-level fallback recovery for implicit cleanup and non-file reassignment cleanup,
broader resource-family cleanup, non-failure backpressure
observation/escalation, pressure observer payload/runtime execution, broader post-acquire resource
operations beyond the covered file iterator, close-method, direct-release, write-method, and
acquired-handle instant-pull paths, failing value-producing resource methods beyond returned file/stdin
   instant pulls, returned explicit matrix-valued function success paths, returned
   local list/dict/matrix container handles, and returned list/dict/matrix bounds slices,
   local resource binding value-producing resource method bodies beyond statement-only called-function bodies,
   resource-method lexical/global captures, and
   arbitrary value-producing resource-effect recovery remain design-fixed but
   unfinished.

### P0. resource topology resource selectors parse, but slice/snapshot value semantics are not closed

Design says `@price[-1]` reads a scalar, while `@price[-3..]` and `@price[...]`
read snapshot/slice values. The active syntax map lists all three forms.

Current implementation reality:

1. The parser has `ResourceSelectorKind::{Whole, Offset, SliceFrom, SnapshotAll}`
   and accepts `@name[-n]`, `@name[-n..]`, and `@name[...]`.
2. Sema now distinguishes selector families for bounded topology
   resources: `@price[-1]` remains the scalar resource value, while bounded
   `i64`, `f64`, `bool`, `char`, `string`, `list`, `dict`, and `matrix` selectors infer
   typed materialized history lists such as `list[i64]`, `list[f64]`,
   `list[bool]`, `list[char]`, `list[string]`, `list[list[T]]`, and
   `list[dict[K,V]]`, and `list[matrix]`; unsupported non-bounded,
   unsupported value-family, or out-of-window selector shapes fail closed.
3. Lowering now handles `ResourceSelectorKind::Offset`,
   `ResourceSelectorKind::SliceFrom`, and `ResourceSelectorKind::SnapshotAll` for
   bounded `i64`, `f64`, `bool`, `char`, `string`, `list`, `dict`, and `matrix`
   resources. Slice/snapshot selectors materialize a list literal from explicit
   history reads instead of falling through to `SGResId::Create(name)`. Logical
   resource writes keep raw handle values on the Topology path instead of
   reusing stdout/file stringification.
4. `tests/features/state_resources/t06_topology_selector_snapshot.styio` proves
   a recent-window resource prints distinct scalar, slice, and snapshot values:
   `@price[-1]` prints `40`, `@price[-2..]` prints `[30,40]`, and `@price[...]`
   prints `[20,30,40]`. `t07_topology_selector_snapshot_f64.styio` proves the
   same value-shape contract for bounded `f64` history, and
   `t08_topology_selector_snapshot_bool.styio` proves bounded `bool` history
   prints `false`, `[true,false]`, and `[false,true,false]`.
   `t10_topology_selector_snapshot_string` proves bounded `string` latest,
   slice, snapshot, and selector-copy values, and
   `t11_topology_selector_snapshot_char.styio` proves bounded `char` latest,
   slice, snapshot, and selector-copy values with `list[char]` rendering.
   `StyioResourceTopology.ResourceSelectorSnapshotIteratorUsesMaterializedValues`
   proves `@price[...] >> #(v)` and `@price[-2..] >> #(v)` iterate over the
   materialized history values instead of a constant-literal fallback, while
   `StyioResourceTopology.ResourceScalarSelectorIteratorFailsClosed` keeps
   `@price[-1] >> ...` rejected as a non-iterable scalar latest read.
   `StyioResourceTopology.ResourceSelectorHandleSnapshotsCloneListAndDictValues`
   proves bounded list/dict resource history snapshots keep independent cloned
   handle values after the source iterator variable mutates, and
   `StyioSecurityResourceTopology.LowersBoundedListAndDictResourceSelectors` proves
   the LLVM path uses list/dict clone, push, get, and owned ring storage.
   `tests/features/state_resources/t12_topology_selector_snapshot_matrix.styio`
   and `StyioResourceTopology.ResourceSelectorMatrixSnapshotsCloneValues` prove
   bounded matrix resource history snapshots keep independent cloned matrix
   handles after the source loop variable mutates, while
   `StyioSecurityResourceTopology.LowersBoundedMatrixResourceSelectors` proves the
   LLVM path uses matrix clone, list push/get, and owned ring storage.
   Adjacent negative fixtures reject selectors deeper than the declared history
   bound and unbounded matrix snapshots.
5. Explicit selector copy now covers bounded scalar/string and list/dict/matrix handle
   selector snapshots:
   `name << @resource[...]` and `name << @resource[-n..]` bind the materialized
   selector list for bounded `i64`, `f64`, `bool`, `char`, `string`, `list`,
   `dict`, and `matrix` resources instead of treating the selector as a write target.
   `t09_topology_selector_explicit_copy`
   proves copied `i64`, `f64`, and `bool` snapshots, `t10` proves copied
   `string` snapshots, `t11` proves copied `char` snapshots, while
   `StyioResourceTopology.ResourceSelectorHandleSnapshotCopiesStayMaterialized`
   proves copied list/dict handle snapshots stay materialized after later writes
   advance the source resource ring, `t12` proves the same selector-copy path
   for matrix handle snapshots, and
   `e07_selector_copy_scalar_unsupported` rejects `name << @resource[-1]`
   because the scalar latest read is not an enumerable snapshot copy.
6. The materialized-container type-directed copy slice now covers
   `copy << list_source`, `copy << dict_source`, and `copy << matrix_source`.
   Those forms lower through list/dict/matrix clone IR and runtime helpers,
   produce independent containers after source mutation, and
   `copy <- list_source` / `copy <- dict_source` / `copy <- matrix_source`
   fail closed because `<-` is acquire/receive or task pull rather than
   bound-resource clone.

Impact: the prior silent scalar/latest-resource collapse is closed for bounded
`i64`, `f64`, `bool`, `char`, `string`, `list`, `dict`, and `matrix` resource selectors,
including explicit copy from scalar/string/list/dict/matrix slice/snapshot selectors, iterator
execution over materialized selector snapshots, and runtime-owned handle
history for list/dict/matrix values; materialized list/dict/matrix handle cloning is
closed for `copy << source`. Broader selector closure still needs unsupported
tuple value-family history storage, unbounded sequence snapshot policy,
and broader type-directed `<<` copy/clone semantics for file, topology-resource, and future resource families
before the complete resource topology selector model can be considered complete.

### P0. Stream concurrency and pressure are only partially executable

Design and IM-D5 specify deterministic pulse frames, zip barrier synchronization,
snapshot joins, declared queue/pressure/timeout/EOF behavior, pressure observers,
and multiple-writer merge/conflict rules.

Current implementation reality:

1. `SIOStreamZip` codegen supports list literal pairs, file-backed stream pairs,
   literal/file mixed pairs, materialized non-file `list[T]` handle pairs, mixed
   `@file` / materialized-list pairs in both directions, and bounded Topology
   selector snapshots that have already materialized as `list[T]` handles.
   `@stdin` now participates as a standard-input line-stream zip source with
   materialized lists or `@file` streams in both source orders, while duplicate
   `@stdin & @stdin` consumption remains fail-closed pending a stream-driver
   decision.
   Runtime list loops use `styio_list_len` / `styio_list_get_*`, cover `i64`,
   `string`, `f64`, `bool`, and `char` list elements plus bounded matrix selector
   snapshots materialized as `list[matrix]`, and terminate finite zip at the
   shorter file EOF, stdin EOF, or list length. `t11_zip_bound_lists` proves the
   parser-shadow-safe bound-list/literal zip path, `t12_zip_file_bound_list`
   proves list-left / file-right feature coverage, and
   `t13_zip_resource_selector_snapshots` proves bounded `i64` and `string`
   selector snapshots feed the same finite zip barrier, while
   `t14_zip_matrix_selector_snapshots` proves the bounded matrix selector
   snapshot path. Focused unit coverage
   proves f64/bool/char materialized-list zip lowering, both mixed file/list
   directions, both mixed stdin/list and stdin/file directions, and keeps latest
   resource selectors such as `@price[-1]` and raw matrix latest selectors plus
   duplicate stdin zip fail-closed as unsupported zip inputs. This is not IM-D5
   snapshot-join semantics; it is the materialized-list zip slice over selector
   snapshot values plus the accepted standard-input line stream.
2. Resource topology records backpressure edges for writes, collect, iterator,
   and zip paths, but that is analysis graph evidence rather than a complete
   runtime scheduling/effect system.
3. Existing stream-processing tests pass for selected fixtures, but they do not
   close the full IM-D5 model.

Impact: the stream core is real for selected examples, but the design's
cross-stream synchronization and pressure semantics remain target architecture.
Do not market or depend on arbitrary stream-source combinations yet.

## High-Priority Compiler Gaps

### P1. Sema/IR has explicit unsupported language families

The recovered compiler is safer than the old placeholder path, but several AST
families still do not have real StyioIR semantics:

1. `TupleAST` lowers to `unsupported AST lowering: TupleAST`.
2. `ExtractorAST` lowers to `unsupported AST lowering: ExtractorAST`.
3. `SetAST` lowers to `unsupported AST lowering: SetAST`.
4. `NoneAST` is rejected because null/none value semantics are not defined.
5. `InfiniteAST`, `ForwardAST`, `BackwardAST`, and `CODPAST` have no active
   StyioIR lowering.
6. `IterSeqAST` fail-closes with a diagnostic telling users to use normal
   `#(param) => { ... }` iterator bodies.

This is mostly good failure behavior, but it is still a design gap wherever the
surface appears in active docs, EBNF, examples, or parser support.

Closed evidence inside this gap: resource method bodies now accept and inline
single-byte `char` literals, format strings, scalar leaf values, match
expressions, and dynamic range literals.
`@file::marker = () => { >_('x') }`,
`@file::summary = () => { $"value={1 + 2}" -> @stdout }`, and
`@file::flag = () => { <| true }`,
`@file::ratio = () => { <| 1.5 }`,
`@file::word = () => { <| "ok" }`,
`@file::summary = (x: int) => { <| $"value={x + 1}" }`,
`@file::answer = () => { >_("inside") <| 42 }`,
`@file::self_path := @file.path`,
`@file::describe = () => { <| $"path={@file.path}" }`,
`@file::pick = (x: int) => { <| x ?= { 0 => 'a' _ => 'b' } }`,
`@file::span = (start: int, stop: int, step: int) => { <| [start..stop..step] }`,
`@file::list_answer = () => { xs := [41,42] <| xs }`,
`@file::dict_answer = () => { d := dict{"a": 40, "b": 2} <| d }`,
`@file::read_or = () => { <| ?| (<< @file("data.txt")) | io => 8 | 7 }`,
and `# make : matrix = () => { <| [[1,2],[3,4]] }` feeding
`@file::make = () => { <| make() }`
run after resource method calls or under `?| method() | fallback`, while
top-level `@file.path` remains a parse error rather than a constructor/property
shortcut,
`@file::marker = () => { >_('xy') }`,
`@file::summary = () => { $"value={1 + 2" -> @stdout }`, and
returned resource-effect discard, returned container match results, or non-integer range bounds fail closed; returned format-string fallback type
mismatches also report `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH`. Single-return resource methods returning
materialized list index/list-slice, inline dict-index, ordered dict value-slice,
or typed-parameter matrix cell/row or row-range slice expressions also inline
through `ListAST`/`ListOpAST` and `DictAST` clone paths with type metadata
preserved, and flat-list matrix returns fail closed before runtime, while
unimplemented lexical/global capture shapes stay fail-closed.
That closes only the `IntAST`, `BoolAST`, `FloatAST`, `StringAST`, `CharAST`, `FmtStrAST`, receiver-scoped `ResourceReceiverAST` / `AttrAST` property postfix, `RangeAST`, `MatchCasesAST`, `ResourceEffectAST`, local list/dict/matrix container-return, and returned
list/dict/matrix bounds resource-method inline-clone slices of the state inline
clone surface; other accepted AST families still need source-reachable evidence
before the unsupported clone fallback can be retired.

### P1. Type semantics still contain recovery-era defaults

Examples:

1. Function return lowering no longer maps tuple return metadata through an
   `i64` fallback: tuple return annotations now fail closed with a type
   diagnostic until tuple value IR exists, and the public JSONL code is the
   feature-owned `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`. Unspecified function
   returns infer final expression tails for the covered scalar families;
   statement-only tails still return the current generated default value.
2. resource topology resource declaration lowering initializes declared slots to a
   zero value by storage type. For `i64`, `f64`, `bool`, `char`, and `string`
   fixed/recent resources this becomes a bounded-ring storage value, but
   unsupported tuple/list/dict/matrix storage and absence/default semantics are
   not a full typed resource initialization contract.
3. Range literals now accept integer expressions for `start`, `end`, and
   optional `step`; non-integer bounds fail in Sema. Constant ranges still lower
   to list literals, while expression-bound ranges materialize `list[i64]`
   through the runtime list loop used by the current value/print paths.
4. Accepted match expressions and function match sugar now run Sema before
   lowering: scrutinees must infer integer type, case patterns stay on the
   integer-literal/equality subset that lowering accepts, arm/default bodies are
   type-inferred in isolated branch scopes, and `i64`/`f64`/`bool`/`char`/`string`
   tail result kinds are recorded on `MatchCasesAST` and preserved through
   `SGMatch` lowering/codegen. Undefined branch tail values, branch-local binding
   leaks, and unsupported container result families now fail as type errors
   instead of reaching codegen as default `i64` values. Broader match result
   families such as tuple/list/dict/matrix still remain future feature work.

Impact: these are not silent `SGConstInt(0)` placeholder AST lowerings, but they
are still places where the design's expression-oriented and typed semantics have
not fully replaced recovery defaults.

### P1. IDE/LSP is useful but intentionally incomplete

Current LSP capabilities match the documented core: completion, hover,
definition, references, document/workspace symbols, semantic tokens, document
sync, and diagnostics.

Still missing by design/current docs:

1. `rename`
2. `codeAction`
3. `inlayHint`
4. multi-workspace/server deployment behavior
5. full independent idle runtime, since stdio service progress is request-driven

Impact: Vityo/first-party adapters may have richer local UX, but public compiler
service truth is narrower. Hosts must not infer unsupported public LSP features
from first-party fallback behavior.

## Medium-Priority Ecosystem Gaps

### P2. Package-manager expectations are split by design, but easy to misread

`styio` currently owns:

1. nano package materialization
2. static local registry consume/publish
3. `compile_plan` producer/consumer contract
4. machine-readable compiler capabilities

Compiler-side nano negative paths now have explicit evidence for the current
handoff surface: malformed static repository entry schemas, malformed cloud
package manifests, create/publish mutual exclusion, missing nano mode selection,
create-only/publish-only option mixups, invalid markers, blob SHA256 mismatches,
blob size mismatches, and HTTP(S) publish-root rejection are covered in
`tests/styio_test.cpp` without adding package-manager lifecycle commands.

Compiler-side `compile_plan` validation now also rejects malformed optional
`profile.build_mode` values instead of silently defaulting them, rejects
non-object package entries, and rejects plans whose `entry.package_id` is absent
from `packages`. The coverage remains limited to the resolved compiler request
envelope and does not add resolver, install, registry, or lifecycle behavior.

`styio` does not own full package lifecycle UX:

1. `install`
2. `use`
3. `search`
4. `vendor`
5. `pin`
6. dependency resolution
7. lockfiles
8. remote registry protocol/auth/trust

This is not a code defect inside `styio`; it is a boundary that must stay visible
in any design review. If a recovered product design assumes those features are
in this repository, that design is currently mapped to the wrong repo.

### P2. Release/conformance evidence is strong locally but not yet full release closure

The local build passed broad language, security, pipeline, algorithm,
performance, and deep-soak checks after building the missing optional targets.
Remaining gap is release posture:

1. Cross-platform release matrix is not proven by this local macOS checkout.
2. Sanitizer/fuzz/perf/nightly lanes are documented as tiered responsibilities,
   but this audit did not run every L3 lane.
3. CTest can show misleading `NOT_BUILT`/missing executable failures if optional
   targets have not been built before selection by regex or all-test runs.

Impact: local recovery health is good, but release promotion should still use the
IM-D6 lane vocabulary and avoid treating "not built in this tree yet" as either
a source defect or a pass.

## Non-Gaps

These should not be counted as missing implementation in this checkout:

1. Full package-manager product behavior belongs to `styio-spio`, not `styio`.
2. Remote registry service semantics, auth/signing/trust, channel aliasing, and
   package listing APIs are not compiler responsibilities.
3. Retired state-resource containers, source-level bare `@`, and retired wave
   tokens should remain rejected unless a new design checkpoint deliberately
   reopens them.
4. The presence of legacy parser code is not by itself evidence that accepted
   grammar still depends on fallback; current parser shadow gates passed for the
   covered suites. It remains maintenance debt, not current user-facing authority.
5. Range literal expression bounds are no longer an implementation gap for
   integer expressions. `RangeLiteralExpressionBoundsMaterializeList`,
   `RangeLiteralRejectsNonIntegerExpressionBounds`, and
   `DynamicRangeLiteralLowersToRuntimeListLoop` prove parser/Sema/lowering/
   codegen/runtime-list behavior and the adjacent float-bound rejection.
6. Nano static repository and package-manifest edge validation is no longer just
   code-only for the current compiler-side handoff surface. The
   `StyioNanoPackage.*` negative-path tests prove malformed entry schemas,
   malformed manifests, blob integrity failures, remote publish rejection, and
   create/publish CLI guard behavior while leaving package lifecycle UX outside
   this repository.
7. File-close cleanup failure is no longer a missing runtime-effect family for
   the explicit file-write resource operation. `StyioResourceEffects` covers
   matched cleanup recovery and adjacent `io` non-match behavior, while
   `StyioSafetyRuntime.FileCloseFailureIsCleanupRuntimeEffect` covers the direct
   runtime subcode/effect-family mapping.
8. Plain file acquire/write/release and file-iterator failures no longer leak
   past their ordinary statement boundary.
   `StyioDiagnostics.RuntimeFileAcquireFailureStopsBeforeNextStatement`,
   `StyioDiagnostics.RuntimeFileWriteFailureStopsBeforeNextStatement`,
   `StyioDiagnostics.RuntimeFileReleaseFailureStopsBeforeNextStatement`, and
   `StyioDiagnostics.RuntimeFileIteratorOpenFailureStopsBeforeNextStatement`
   prove JSONL runtime diagnostics are emitted and a following `after` print is
   not executed outside a `?|` recovery wrapper.
   `StyioResourceLifecycle.DirectFileReleaseSuccessContinues` proves successful
   direct file release proceeds to the next statement.
   `StyioResourceLifecycle.FileAliasUseAfterCloseFailsFast` proves same-path
   alias use after close now reports a closed-handle diagnostic instead of
   normal EOF.
9. Explicit return no longer bypasses tracked file cleanup. `SGReturn` emits
   file-handle cleanup before the LLVM `ret`,
   `StyioSecurityNightlyCodegen.ReturnRunsFileScopeCleanupBeforeRet` proves the
   emitted close happens before the function return, and
   `StyioResourceLifecycle.FunctionReturnRunsFileScopeCleanupSmoke` keeps the
   source-level early-return path executable.
10. Tuple function return annotations are no longer a silent `i64` fallback.
   `StyioDiagnostics.TupleFunctionReturnAnnotationReportsTypeCode` proves the CLI
   JSONL `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN` path,
   `ScalarAndInferredFunctionReturnsStayExecutable` keeps adjacent scalar/inferred returns executable, and
   `StyioSecurityNightlyParserStmt.RejectsTupleFunctionReturnAnnotationBeforeLoweringFallback`
   covers the Sema/lowering fail-closed path.
11. Task await fallback settlement is no longer missing from the resource-effect
   compatibility path. `StyioResourceEffects` proves failed task pulls run
   fallback after clearing the materialized task error and fail fast without
   fallback, `StyioSecurityNightlyParserStmt` keeps bare continuation freeze
   fallback fail-closed, and `task_resources` feature negatives cover non-task
   await sources and reserved bare freeze fallback syntax.
12. File and stdin instant-pull value-producing resource effects are no longer missing
   from the non-task `?|` path. `StyioResourceEffects` proves success and
   fallback values are returned from `result = ?| (<< @file("data.txt")) | fallback`,
   named `io` handler values can recover a file-open read failure, and acquired
   file handles from a preceding `?| f <- @file(...) | ...` can feed
   `result = ?| (<< f) | fallback` through a handle-backed runtime helper.
   Recovered failed acquires followed by `?| (<< f) | closed => 9 | 7` recover
   `STYIO_RUNTIME_INVALID_FILE_HANDLE`, the no-fallback acquired-handle pull
   stops before the following statement, and non-file names such as materialized
   lists fail closed before lowering. Stdin
   `result = ?| (<- @stdin) | fallback` recovers numeric parse failures through
   fallback or matched `parse` handlers, explicit-target stdin `f64`, `string`,
   and `list[i64]` resource-effect values execute through the same value PHI
   path, and no-fallback expression settlement fails before the following
   statement.
   `StyioSecurityNightlyParserStmt` / `StyioSecurityNightlySemantics` prove the
   parser/codegen value path and keep expression discard, statement-shaped write
   expressions, non-file acquired-handle pull sources, and mismatched fallback
   values fail-closed.
13. Resource method calls and file handle-acquire statements are no longer
   excluded from statement-shaped resource-effect settlement.
   `StyioResourceEffects` proves direct
   `@file(...).close()` skips fallback on success, recovers missing file
   open-read failures through catch-all fallback or a matched `io` handler, and
   fails fast without fallback before a following statement. The same suite now
   proves `?| f <- @file(missing) | fallback` recovers open-read failures
   through catch-all fallback or a matched `io` handler, skips fallback on a
   successful open, keeps the acquired file handle usable for a following
   iterator, later resource-effect close method, later guarded write method, or
   later value-producing instant pull,
   fails fast without fallback before a following statement, and
   fails closed with `STYIO_RUNTIME_INVALID_FILE_HANDLE` when fallback recovers
   the open failure but later code tries to iterate, write through, or instant
   pull from the zeroed handle without recreating the file path.
   The same statement route now covers file flex rebind:
   `StyioResourceEffects.FlexRebindFallbackRecoversFileOpenFailure` and
   `FlexRebindNamedIoHandlerRecoversFileOpenFailure` prove `?| f = @file(missing) | ...`
   recovers replacement open failures and leaves the later acquired-handle pull
   on the closed-handle path, while
   `StyioSecurityNightlyParserStmt.ParsesResourceEffectFileRebindStatement`,
   `RejectsScalarFlexBindResourceEffectStatement`, and
   `RejectsFileRebindResourceEffectExpression` prove the no-fallback parser
   route, scalar-assignment rejection, value-expression rejection, and cleanup
   branch codegen shape.
   `StyioSecurityNightlyParserStmt.ParsesResourceEffectResourceMethodStatement`
   and `ParsesResourceEffectHandleAcquireStatement` /
   `ResourceEffectHandleAcquireFeedsLaterIterator` /
   `ResourceEffectHandleAcquireFeedsLaterCloseMethod` /
   `ResourceEffectHandleAcquireFeedsLaterInstantPull` prove
   parser/lowering/codegen routing for the iterator, later close-method, and
   acquired-handle instant-pull
   paths, while
   `StyioSecurityNightlySemantics.RejectsNonResourceMethodResourceEffectStatement`
   keeps ordinary member calls fail-closed under `?|` and
   `ResourceEffectAcquireThenCloseConsumesReceiver` proves the acquired handle
   remains destroyed after a later close.
   `StyioSecurityNightlyParserStmt.RejectsHandleAcquireResourceEffectExpression`
   keeps statement-shaped acquire out of value-required `?|` expressions.
14. Materialized container index/list-slice/dict-value-slice resource effects are no longer
   excluded from the first non-instant-pull `?|` value path.
   `StyioResourceEffects` proves `result = ?| xs[i] | fallback`,
   `result = ?| d[key] | fallback`, `values = ?| d[start..end] | fallback`,
   `result = ?| m[row][col] | fallback`, and
   `row = ?| m[row] | fallback` return successful container values, and
   `slice = ?| xs[0..] | fallback` returns a successful materialized list slice.
   Dict value slices return ordered value lists by reusing `d.values` plus
   `SCListSlice`. The same group proves container reads and list/dict value slices recover
   `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, or
   `STYIO_RUNTIME_MATRIX_INDEX` through catch-all fallback or a matched
   `bounds` handler, and fail fast without a fallback before the following
   statement. `StyioDiagnostics` proves plain `xs[i]`, `xs[0..]`, `d[key]`,
   `d[start..end]`, and `m[row][col]` outside `?|` now emit JSONL bounds diagnostics and stop
   before the following statement, while `StyioSecurityNightlyParserStmt` /
   `StyioSecurityNightlySemantics` prove parser/codegen routing, fallback type
   checking, and the adjacent fallback-mismatch boundary.
15. Simple value-producing resource methods are no longer excluded from the
   first non-task `?|` value path. `StyioResourceEffects` proves direct
   `log.answer()` and `result = ?| log.answer() | fallback` return simple
   single-`<| expr` method values and accepted statement-preface final-return
   bodies without lowering an expression-context
   `SGReturn`, scalar/string match-expression return families preserve their
   inferred type while returned container match results fail closed, fallback
   type mismatches fail closed, and scalar local `=` / `:=` plus local
   list/dict/matrix `=` / `:=` preface method bodies that return scalar/string values
   or local list/dict/matrix container handles return through direct and guarded calls
   without leaking same-named caller bindings; returned local handles are cloned
   before method-block cleanup. Local resource binding prefaces remain
   rejected before lowering. Returned calls to ordinary functions with a value tail or
   explicit `<| expr` also preserve the called function result family through
   direct and guarded resource method calls; statement-only called functions
   remain rejected with the same unsupported-body diagnostic. The same group now proves a method
   whose single return is a file instant pull can recover a returned
   `STYIO_RUNTIME_FILE_OPEN_READ` through catch-all fallback or a matched
   `io` handler, while no-fallback settlement fails before the following
   statement.
16. Match case semantics no longer bypass Sema before lowering. `MatchCasesAST`
   stores the inferred `i64`/`f64`/`bool`/`char`/`string` result family,
   function-body inference runs with a recursion guard so function match sugar
   is checked when called, and each match arm/default is inferred in an isolated
   branch scope. Runtime smoke coverage keeps branch-local match tails executable
   and preserves `bool`/`char` print behavior, while security coverage rejects
   undefined match arm values, unsupported container branch results, and
   branch-local binding leakage for ordinary match expressions, source-reachable
   single-return resource method match bodies, and function match sugar.
17. Stream zip unsupported-source diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies the stable
    non-iterable zip message as `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE`, while
    `StyioDiagnostics.StreamZipUnsupportedSourceReportsFeatureCode` and
    `StyioStreamZip.ResourceScalarSelectorFailsClosedAsZipInput` prove ordinary
    scalar inputs and scalar resource selectors still fail closed with the
    feature-owned JSONL code. This is diagnostic refinement only; duplicate
    `@stdin & @stdin`, hash-tag stream routes, snapshot joins, pressure policy,
    and broader stream-driver semantics remain at their existing open or
    pending boundaries.
18. Duplicate stdin zip diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies the stable
    duplicate stream-driver decision-boundary message as
    `STYIO_TYPE_STREAM_DUPLICATE_DRIVER_UNSUPPORTED`, while
    `StyioDiagnostics.DuplicateStdinZipReportsFeatureCode` proves duplicate
    `@stdin & @stdin` stream zip inputs still fail closed with public phase
    `type`, the TypeError exit family, a stable message fragment, and
    no-following-output behavior. This is diagnostic refinement only; it does
    not implement duplicate external-input consumption, broader stream-driver
    routing, snapshot joins, pressure policy, or IM-D5 duplicate-driver
    semantics.
19. Iterator unsupported-source diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies the stable
    non-iterable iterator message as `STYIO_TYPE_ITERATION_UNSUPPORTED_SOURCE`,
    while `StyioDiagnostics.IteratorUnsupportedSourceReportsFeatureCode` and
    `StyioResourceTopology.ResourceScalarSelectorIteratorFailsClosed` prove ordinary
    scalar iterator sources and scalar latest resource selectors still fail
    closed with public phase `type`, the TypeError exit family, stable message
    fragments, and no-following-output behavior. This is diagnostic refinement
    only; it does not broaden iterator source support, selector semantics,
    hash-tag stream routes, snapshot joins, or stream-driver behavior.
20. Immutable/final binding mutation diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies stable
    final-binding mutation messages such as compound assignment to a final
    binding as `STYIO_SEMA_IMMUTABLE_BINDING`, while
    `StyioDiagnostics.CompoundAssignOnImmutableBindingReportsSemaCode` proves
    the JSONL public phase is `sema`, the exit family remains TypeError, and
    the stable message fragment is still present. This is diagnostic refinement
    only; it does not broaden or change final-binding mutability semantics.
21. Tuple function return annotation diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies the
    stable tuple-value-IR rejection message as
    `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`, while
    `StyioDiagnostics.TupleFunctionReturnAnnotationReportsTypeCode` proves the
    JSONL public phase is `type`, the exit family remains TypeError, and the
    stable message fragment is still present. This is diagnostic refinement
    only; tuple value IR and tuple return execution remain open language/runtime
    work.
22. Undefined hash-tag stream route diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies the
    stable fail-closed iterator-sequence message as
    `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`, while
    `StyioDiagnostics.IteratorSequenceHashTagRoutingReportsFeatureCode` proves
    the JSONL public phase is `type`, the exit family remains TypeError, and
    execution still stops before the following statement. This is diagnostic
    refinement only; IM-D5-P1 still owns whether hash-tag routes are retired or
    defined, and no hash-tag route semantics are implemented.
23. Resource-effect value and resource-method body diagnostics are no longer
    only broad `STYIO_TYPE_ERROR` cases at two existing fail-closed boundaries.
    `DiagnosticContract.hpp` now classifies stable resource-effect fallback
    mismatch messages as `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH` and
    unsupported resource method bodies such as statement-only called-function
    returns and local resource binding method bodies as
    `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY`. The focused
    `StyioResourceEffects.ResourceMethodValueFallbackTypeMismatchReportsTypeCode`
    and `StyioResourceEffects.ResourceMethodReturnedStatementOnlyFunctionReportsSemaCode`
    tests prove the public phases are `type` and `sema`, the TypeError exit
    family remains stable, stable message fragments are present, and following
    output does not execute. This is diagnostic refinement only; broader
    resource method body semantics, resource-method captures, pressure observers,
    and arbitrary resource-effect recovery remain
    open.
24. Native interop source/signature diagnostics are no longer only broad
    type/native fallback cases at two existing fail-closed boundaries.
    `DiagnosticContract.hpp` now classifies missing referenced native source
    files as `STYIO_NATIVE_SOURCE_READ_FAILED` and explicit binding/signature
    misses as `STYIO_NATIVE_SIGNATURE_NOT_FOUND`. The focused
    `StyioDiagnostics.NativeExternMissingSourceReportsNativeCode` and
    `StyioDiagnostics.NativeExternMissingBindingReportsNativeCode` tests prove
    the JSONL public phase is `native_interop`, the TypeError exit family
    remains stable, stable message fragments are present, and following output
    does not execute. This is diagnostic refinement only; native ABI support,
    symbol visibility, host compiler behavior, C++ symbol mapping, and broader
    native effect semantics remain unchanged.
25. Native interop unsupported-signature diagnostics are no longer only broad
    native fallback cases at existing fail-closed signature boundaries.
    `DiagnosticContract.hpp` now classifies unsupported native parameter/return
    families, void parameters, empty/cannot-parse parameters, and variadic
    signatures as `STYIO_NATIVE_UNSUPPORTED_SIGNATURE`. The focused
    `StyioDiagnostics.NativeExternUnsupportedSignatureReportsNativeCode` and
    `StyioDiagnostics.NativeExternVariadicSignatureReportsNativeCode` tests
    prove aggregate parameter and variadic-signature failures report public
    phase `native_interop`, keep the TypeError exit family stable, preserve
    stable message fragments, and stop before following output. This is
    diagnostic refinement only; aggregate, variadic, broader C/C++ ABI, symbol
    visibility, and host compiler behavior remain unchanged.
26. Native interop host compile diagnostics are no longer only broad native
    fallback cases at the existing host compiler rejection boundary.
    `DiagnosticContract.hpp` now classifies host compiler rejection messages as
    `STYIO_NATIVE_HOST_COMPILE_FAILED`, while
    `StyioDiagnostics.NativeExternHostCompileFailureReportsNativeCode` proves
    the JSONL public phase is `native_interop`, the TypeError exit family
    remains stable, the stable compile-failed message fragment is present, and
    following output does not execute. This is diagnostic refinement only; host
    compiler behavior, native ABI support, signature support, symbol visibility,
    artifact loading, and C++ symbol mapping remain unchanged.
27. Native interop load/symbol/toolchain diagnostics are no longer only broad
    native fallback cases at existing artifact-load, exported-symbol, and
    toolchain-configuration boundaries. `DiagnosticContract.hpp` now classifies
    native artifact `dlopen` failures as `STYIO_NATIVE_LOAD_FAILED`, missing
    exported symbols as `STYIO_NATIVE_SYMBOL_MISSING`, and unavailable or invalid
    native toolchain configuration as `STYIO_NATIVE_TOOLCHAIN_UNAVAILABLE`. The
    focused `StyioDiagnostics.NativeExternLoadFailureReportsNativeCode`,
    `StyioDiagnostics.NativeExternSymbolMissingReportsNativeCode`, and
    `StyioDiagnostics.NativeExternToolchainUnavailableReportsNativeCode` tests
    prove phase `native_interop`, the TypeError exit family, stable message
    fragments, and no-following-output behavior, including a fake-compiler route
    that produces an invalid shared object and a static-symbol route that fails
    lookup without changing symbol visibility. This is diagnostic refinement only;
    native artifact loading, symbol visibility, C++ symbol mapping, ABI support,
    signature support, and host toolchain behavior remain unchanged.
28. Unknown function/resource diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies stable
    `unknown function` and `unknown resource` messages as
    `STYIO_SEMA_UNDECLARED_SYMBOL`, while
    `StyioDiagnostics.UnknownFunctionReportsSemaUndeclaredSymbolCode` and
    `StyioDiagnostics.UnknownResourceReportsSemaUndeclaredSymbolCode` prove the
    JSONL public phase is `sema`, the TypeError exit family remains stable,
    stable message fragments are present, and following output does not execute.
    This is diagnostic refinement only; it does not broaden symbol resolution,
    hidden native symbol visibility, import behavior, or resource lookup
    semantics.
29. User function and resource-method call arity diagnostics are no longer only
    the broad `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies
    stable `expects N argument(s), got M` messages as
    `STYIO_SEMA_CALL_ARITY_MISMATCH`, while
    `StyioDiagnostics.UserFunctionArityReportsSemaCallArityMismatchCode` and
    `StyioDiagnostics.ResourceMethodArityReportsSemaCallArityMismatchCode`
    prove the JSONL public phase is `sema`, the TypeError exit family remains
    stable, stable message fragments are present, and following output does not
    execute. This is diagnostic refinement only; it does not broaden function
    calling or resource-method dispatch semantics.
30. User function and resource-method call argument type mismatch diagnostics are
    no longer only the broad `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp`
    now classifies stable `function argument type mismatch for parameter` and
    `resource method argument type mismatch for parameter` messages as
    `STYIO_TYPE_CALL_ARGUMENT_MISMATCH`, while
    `StyioDiagnostics.UserFunctionArgumentMismatchReportsTypeCallArgumentMismatchCode`
    and
    `StyioDiagnostics.ResourceMethodArgumentMismatchReportsTypeCallArgumentMismatchCode`
    prove the JSONL public phase is `type`, the TypeError exit family remains
    stable, stable message fragments are present, and following output does not
    execute. This is diagnostic refinement only; it does not broaden function
    calling, implicit argument adaptation, native ABI behavior, or resource-method
    dispatch semantics.
31. Resource capability mismatch diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family for the focused standard-resource routes.
    `DiagnosticContract.hpp` now classifies stable capability messages such as
    write-to-`@stdin` and instant-pull-from-`@stderr` failures as
    `STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH`, while
    `StyioDiagnostics.ResourceWriteToStdinReportsSemaCapabilityCode` and
    `StyioDiagnostics.InstantPullFromStderrReportsSemaCapabilityCode` prove the
    JSONL public phase is `sema`, the TypeError exit family remains stable,
    stable message fragments are present, and following output does not
    execute. This is diagnostic refinement only; it does not broaden resource
    capability rules, resource family support, pressure observers, or fallback
    recovery semantics.
32. Unsupported typed stdin target diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family. `DiagnosticContract.hpp` now classifies the
    stable unsupported scalar and list target messages as
    `STYIO_TYPE_STDIN_UNSUPPORTED_TARGET`, while
    `StyioDiagnostics.UnsupportedTypedStdinTargetReportsTypeCode` and
    `StyioDiagnostics.UnsupportedTypedStdinListTargetReportsTypeCode` prove the
    JSONL public phase is `type`, the TypeError exit family remains stable,
    stable message fragments are present, and following output does not
    execute. This is diagnostic refinement only; it does not broaden supported
    stdin target families or resource-effect recovery semantics.
33. Matrix literal shape/context diagnostics are no longer only the broad
    `STYIO_TYPE_ERROR` family for the focused typed matrix literal routes.
    `DiagnosticContract.hpp` now classifies stable matrix literal messages such
    as inconsistent row lengths or flat-list matrix return tails as
    `STYIO_TYPE_MATRIX_LITERAL_INVALID`. `StyioDiagnostics.MatrixBindingRaggedLiteralReportsTypeCode`
    and `StyioDiagnostics.MatrixFunctionReturnFlatListFailsBeforeRuntime` prove
    the JSONL public phase is `type`, the TypeError exit family remains stable,
    stable message fragments are present, and following output does not
    execute. This is diagnostic refinement only; it does not broaden matrix
    literal acceptance, matrix return compatibility, matrix operators,
    resource-method matrix bodies, or matrix runtime behavior.

## Recommended Closure Order

1. Complete the remaining `?| resource_operation` forms in resource-effect
   checkpoints. Statement-only discard, catch-all fallback, and the first
   statement-shaped named-handler chain slice are implemented, and explicit
   file-write close cleanup failure now reaches the `cleanup` handler family.
   Task_await fallback settlement now has parser, Sema, lowering, runtime, and
   negative evidence. Plain file acquire/write/release, direct file iterator
   open, and same-path alias closed-handle failures now settle at ordinary
   statement boundaries outside `?|`, and file/stdin instant-pull plus materialized
   container-index/list-slice resource-effect expressions and simple
   value-producing resource methods now return typed
   success/fallback/handler values, including explicit-target stdin `f64`,
   `string`, typed-list values, list slices, matrix row-range slices, list/dict/matrix `bounds`
   recovery, returned file instant-pull `io` recovery, and returned canonical
   stdin instant-pull `parse` recovery. Resource
   method calls now enter statement `?|` settlement for direct file close
   success, fallback/`io` recovery, and no-fallback failure. Direct file release
   to `@()` now also enters statement `?|` settlement for success,
   fallback/`io` recovery, no-fallback failure, acquired-handle direct release,
   receiver invalidation after release, and value-context rejection; statement-shaped
   direct file iterators now cover success, fallback/`io` recovery, no-fallback
   failure, and non-file iterator rejection; statement-shaped
   file acquire now covers fallback/`io` recovery, successful acquire followed by
   file iteration, a later resource-effect close method, a later guarded file
   write method, or a later acquired-handle instant pull, no-fallback failure
   for file-open read errors, fail-closed later iterator, write-method, or
   instant-pull use after a recovered failed acquire, and
   close-method receiver invalidation. Explicit returns and loop break/continue
   exits now close tracked file handles before leaving the function or loop
   resource scope. The next slices must cover
   source-level fallback recovery for implicit cleanup and non-file reassignment cleanup, broader
   post-acquire resource operations beyond the covered file iterator,
   close-method, write-method, and acquired-handle instant-pull paths,
   additional resource families that emit typed pressure or cleanup
   effects, and arbitrary value-producing recovery beyond the covered paths.
2. Continue resource topology selector value semantics before adding new resource
   features: bounded `i64`, `f64`, `bool`, `char`, `string`, `list`, `dict`, and `matrix`
   selector storage is closed, while bounded selector `snapshot << @x[...]` /
   `snapshot << @x[-n..]` copy is closed for the scalar/string/list/dict/matrix families, and
   materialized list/dict/matrix handle `copy << source` now has deep-copy
   runtime evidence. Unsupported tuple history storage, unbounded
   sequence snapshots, and broader type-directed `<<` copy/clone for file,
   topology-resource, and future resource families still need distinct sema
   types, lowering, runtime values, and golden tests.
3. Continue stream-source closure after the materialized list-handle,
   bounded-selector-snapshot, and stdin line-stream zip slices: true snapshot joins, pressure
   observers, timeouts, EOF/failure distinctions, and merge/conflict semantics
   still need dedicated checkpoints across parser, sema, lowering, runtime,
   topology graph, diagnostics, and tests.
4. Retire or implement the explicit unsupported AST families according to the
   active syntax docs. Do not let parsed-but-unlowerable forms accumulate.
5. Expand IDE only after service facts and diagnostics remain stable under the
   compiler-owned semantic bridge. `rename` and `codeAction` should be backed by
   shared service facts, not editor-local grammar guesses.
6. Keep package lifecycle scope out of `styio`; update handoff docs when `spio`
   contracts change instead of adding compiler CLI flags opportunistically.
7. Make release gates resilient to optional-target discovery by ensuring the
   default documented build target set produces every test binary that default
   CTest invocations are expected to run.
