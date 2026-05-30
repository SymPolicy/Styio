# IM-D4 Resource Management Inventory

**Purpose:** Record the accepted resource-management decisions for IM-D4 without duplicating the IM-D1 StyioIR verifier, lowering, no-op, or codegen-gate contract.

**Last updated:** 2026-05-30

## Scope

IM-D4 owns the language contract for resources as first-class subjects with identity, lifetime, capabilities, protocol state, cleanup behavior, block-entry snapshot behavior, commit behavior, and fallible operations.

This document is intentionally narrower than "all resource-related implementation work." It does not decide where the verifier pass lives, whether codegen must accept only verified StyioIR, how parser fallback is rejected, or how public diagnostic codes are named. Those are already covered by IM-D1, IM-D2, and IM-D3.

## Current State

Topology v2 gives Styio an active source direction for resource declarations, writes, block execution, and selectors:

- `@name : Type` declares a named resource.
- `expr -> @name` writes into a resource sink.
- block-entering forms such as `>> { ... }`, `=> { ... }`, `?= { ... }`, and task blocks enter a resource snapshot context.
- `@name[-1]`, `@name[-3..]`, and `@name[...]` read resource snapshots or slices.
- `@stdin`, `@stdout`, `@file(...)`, and similar entries act as standard or external resource anchors.
- `?| resource_operation`, `?| resource_operation | fallback`, and
  `?| resource_operation | ...` now have an explicit statement-level
  resource-effect AST/IR wrapper. The fallback slice is executable for
  statement-shaped resource operations backed by the current runtime error
  channel: file-write failure runs the fallback after clearing the materialized
  error, successful writes skip fallback, and no-fallback settlement stops at
  the source site. Statement-shaped named handlers and handler chains are also
  executable for current runtime subcode families: `io`, `parse`, `bounds`,
  `closed`, and file-close `cleanup` dispatch by matching the materialized
  runtime subcode, while `backpressure` remains an accepted handler name that
  does not match until a resource family emits that typed effect. Duplicate and
  unknown handler names fail closed, `effect => @()` remains invalid, unmatched
  handlers fall through to the next handler or final catch-all fallback, and
  unmatched failures without fallback keep the default fail-fast rule. This
  checkpoint also recognizes the pressure-observer surface
  `resource.pressure >> #(p) => { ... }` through the nightly parser and routes it
  to Sema; because no current resource family declares a pressure stream,
  `@stdout.pressure` and topology resource bindings such as
  `channel.pressure` fail closed with
  `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`. This
  preserves task_await binding for `?| task -> value: T`; failed task pulls now
  run the await fallback after clearing the materialized task error, while
  failed task pulls without fallback stop at the await settlement site. Non-task
  await sources and bare continuation freeze fallbacks remain fail-closed.
  Statement-shaped resource method calls now enter the same settlement route:
  `?| @file("data.txt").close() | fallback` skips recovery on successful file
  open/close, missing files recover through catch-all fallback or matched `io`
  handlers after the materialized open-read failure is cleared, and no-fallback
  close settlement raises `STYIO_RUNTIME_FILE_OPEN_READ` before the next
  statement. Statement-shaped file handle acquire now enters the same recovery
  route for file resources: `?| f <- @file(missing) | fallback` and matched
  `io` handlers recover open-read failures, a successful open skips fallback,
  and no-fallback settlement raises `STYIO_RUNTIME_FILE_OPEN_READ` before the
  next statement. The successful acquire path now participates in resource
  topology binding, so a following `f >> #(line) => { ... }` iterator can use
  the handle, a later `?| f.close() | fallback` resource-effect close can
  consume it, a later guarded `?| f.write(...) | ...` write-method path can
  write through the acquired resource subject, and a later
  `value = ?| (<< f) | fallback` instant-pull expression can read the next
  `i64` line from that same acquired handle. After that close, topology still
  marks the receiver destroyed so following `f.path` use fails closed. If
  fallback recovered the open failure and later code tries to iterate the
  zeroed slot, the iterator reports `STYIO_RUNTIME_INVALID_FILE_HANDLE`. If
  later code tries `f.write(...)` or `?| (<< f) | closed => value` through the
  same zeroed slot, the operation reports `STYIO_RUNTIME_INVALID_FILE_HANDLE`,
  may recover through a matched `closed` handler, stops before the following
  statement when no fallback is present, and the write method does not recreate
  the file by reopening the saved path.
  Statement-shaped file flex rebind now uses the same settlement route for
  `?| f = @file(next) | fallback`: the parser admits only the file-resource
  rebind shape, Sema keeps scalar `?| x = 1 | ...` rejected, runtime file-open
  failures recover through catch-all fallback or matched `io` handlers, and the
  codegen cleanup branch leaves a prior tracked-handle close failure on the
  `SIOResourceEffect` error channel before any replacement open can run.
  Non-resource member calls such as
  `text.lines()` remain rejected under `?|`, and statement-shaped acquire stays
  rejected where a value-producing `?|` expression is required; statement-shaped
  file rebind is rejected in value-required `?|` expressions as well. Broader
  post-acquire resource operations beyond the covered file iterator and
  close-method, write-method, and acquired-handle instant-pull paths still need
  separate implementation checkpoints.
- The first value-producing non-task resource-effect expression slices are
  executable for file/stdin instant pulls, materialized container bounds reads
  including materialized list slices, and simple value-returning resource
  methods.
  `result = ?| (<< @file("data.txt")) | fallback`
  returns the successful `i64` file line value on success, evaluates the fallback
  after clearing a materialized file-open read failure, and can recover through a
  matched named handler such as `io => 9`. After a successful
  `?| f <- @file("data.txt") | ...`, `result = ?| (<< f) | fallback` reads an
  `i64` from the acquired file handle; if the acquire failure was recovered and
  the slot is still zero, a matched `closed => value` handler can recover
  `STYIO_RUNTIME_INVALID_FILE_HANDLE`, while no-fallback settlement stops before
  the next statement. `result = ?| (<- @stdin) | fallback`
  returns a parsed stdin `i64` on success and recovers numeric parse failures
  through catch-all fallback or a matched `parse => handler` after the
  materialized parse error is cleared. Explicit target types now drive stdin
  value-required pulls in the same expression form: `result: f64 = ?| (<- @stdin) | fallback`
  returns or recovers `f64` values, `result: string = ?| (<- @stdin) | fallback`
  returns cloned stdin lines, and `result: list[i64] = ?| (<- @stdin) | fallback`
  materializes typed stdin list values or recovers list-parse failures through
  fallback. `result = ?| xs[i] | fallback`, `result = ?| xs[0..] | fallback`,
  `result = ?| d[key] | fallback`, `values = ?| d[start..end] | fallback`,
  `result = ?| m[row][col] | fallback`, and
  `row = ?| m[row] | fallback` return successful materialized container values;
  `rows = ?| m[start..end] | fallback` returns a materialized row-range
  `list[list[T]]`. These paths recover `STYIO_RUNTIME_LIST_INDEX`,
  `STYIO_RUNTIME_DICT_KEY`, or `STYIO_RUNTIME_MATRIX_INDEX` through catch-all
  fallback or a matched `bounds => handler`, and fail fast without fallback.
  Ordered dict value slices lower through `d.values` plus `SCListSlice`, so
  out-of-range slice bounds use `STYIO_RUNTIME_LIST_INDEX` / `bounds` while key
  lookup still uses `STYIO_RUNTIME_DICT_KEY`. Plain `xs[i]`, `xs[0..]`,
  `d[key]`, `d[start..end]`, `m[row][col]`, and `m[start..end]`
  outside `?|` now have the same default runtime guards before the following
  statement. Expression
  discard remains rejected, and statement-shaped write operations remain rejected
  where a value is required.
  User-defined resource methods with a single `<| expr` body now carry that
  expression's inferred result type, direct calls such as `log.answer()` return
  the inlined value without emitting an expression-context `SGReturn`, and
  `result = ?| log.answer() | fallback` produces the method value on success
  while rejecting fallback type mismatches. Returned bool, f64, char, string,
  and format-string expressions preserve their value family through direct calls
  and guarded value paths, including `@file::summary = (x: int) => { <| $"value={x + 1}" }`.
  Returned match expressions such as `@file::pick = (x: int) => { <| x ?= { 0 => 'a' _ => 'b' } }`
  preserve the current `i64`/`f64`/`bool`/`char`/`string` match result families
  through direct calls and guarded value paths; returned container match results
  remain rejected before lowering.
  Returned dynamic range literals such as `<| [start..stop..step]` inline as
  ordinary `list[i64]` success values and still reject non-integer bounds before lowering.
  Returned value-producing resource-effect expressions such as
  `<| ?| (<< @file("data.txt")) | io => 8 | 7` parse through the authoritative
  resource-effect expression route inside the method body, preserve the
  inferred result type during inline cloning, return successful values through
  direct method calls, and recover through their own fallback or named handler
  before an outer `?| method() | fallback` sees a successful method value.
  Returned resource-effect discard remains rejected because `?| op | ...` is
  statement-only.
  Returned explicit matrix-valued ordinary functions such as
  `# make : matrix = () => { <| [[1,2],[3,4]] }` now preserve matrix handles
  through direct matrix bindings, resource-method calls, and guarded matrix
  fallback literals; flat-list matrix returns such as `<| [1,2]` fail closed in
  Sema before reaching runtime.
  Public JSONL classification now maps
  this fallback mismatch to `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH` and
  the existing unsupported multi-statement value-producing resource method body
  boundary to `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY`; this is an IM-D3
  diagnostic refinement only and does not broaden resource-effect or resource
  method semantics. When that single returned expression
  is a file instant pull, `result = ?| log.read_missing() | fallback` recovers
  `STYIO_RUNTIME_FILE_OPEN_READ` through catch-all fallback or a matched `io`
  handler, and no-fallback settlement fails before the following statement.
  When that expression is canonical `(<- @stdin)`,
  `result = ?| log.read_stdin() | fallback` returns successful parsed `i64`
  values, recovers `STYIO_RUNTIME_NUMERIC_PARSE` through catch-all fallback or a
  matched `parse` handler, and fails fast without fallback. When that returned
  expression is a materialized list index, materialized list slice, inline
  dict index, ordered dict value slice, or typed-parameter matrix cell/row or row-range slice read,
  `?| method() | fallback`
  recovers `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, or
  `STYIO_RUNTIME_MATRIX_INDEX` through catch-all fallback or a matched
  `bounds` handler, and no-fallback dict-key or matrix-index settlement fails
  before the following statement. Multi-statement resource method returns,
  resource-method lexical/global captures,
  and failing value-producing resource-method recovery beyond the
  covered returned file/stdin instant pulls, returned resource-effect expression
  wrappers, returned explicit matrix-valued function success paths, plus
  returned list/dict/matrix bounds slices remain open.
- Plain file resource operations outside a `?|` recovery wrapper now settle at
  the ordinary statement boundary for the covered file acquire/write/release/
  iterator paths: missing `@file` acquire, missing-directory file write, direct
  `@file(missing).close()`, and direct `@file(missing) >> #(line)` failures
  emit structured runtime diagnostics and stop before a following statement.
  Successful direct file release continues to the next statement. File iterator
  lowering now checks the runtime error channel before treating a null line as
  EOF, so use through a
  same-path alias after another alias closes the shared slot reports
  `STYIO_RUNTIME_INVALID_FILE_HANDLE` instead of silently ending the iterator.
  The operation-local guard is suppressed while `SIOResourceEffect` is
  dispatching fallback or named handlers, so explicit `?| ... | fallback`
  recovery remains the recovery surface, including statement-shaped
  `?| f <- @file(missing) | fallback` file-acquire recovery and
  `?| f = @file(missing) | fallback` file-rebind recovery.
- Tracked file handles now have the first compiler-owned scope-exit cleanup
  settlement slice: explicit `<| return` closes active file-handle slots before
  emitting the LLVM `ret`, normal scope-pop cleanup checks the runtime error
  channel after cleanup, loop `^` break and standalone `>>` continue branches
  clean the resource scopes they bypass before jumping to the loop target, and
  function codegen keeps function-local resource scope stacks isolated from
  surrounding codegen. File flex-rebind also runs the tracked file-handle close
  before the RHS acquire/overwrite, then checks the cleanup error channel so
  cleanup failure stops at the rebind boundary; under
  `?| f = @file(...) | fallback` or a matched `cleanup` handler, the same
  materialized cleanup failure is routed through `SIOResourceEffect` before the
  replacement open. File handle slots are allocated
  so later same-path singleton reuse across loop branches still satisfies LLVM
  dominance. This covers tracked file handles on ordinary scope pop, explicit
  return, loop break/continue exits, default no-fallback file flex-rebind
  cleanup settlement, and explicit statement-shaped file flex-rebind
  settlement; it does not yet provide a source-level fallback recovery site for
  implicit cleanup failures, non-file reassignment cleanup failures,
  release/commit hooks, or non-file cleanup families.
- Bounded `i64`, `f64`, `bool`, `char`, `string`, `list`, `dict`, and `matrix` resource
  selectors now have distinct executable value shapes: `@name[-n]` reads the
  latest/history value, while `@name[-n..]` and `@name[...]` materialize typed
  list snapshots from explicit history reads. Char rings store `i8` values and
  materialized `list[char]` snapshots render escaped char literals; string
  selector rings own cloned cstr values and release them on overwrite or scope
  cleanup; list/dict/matrix selector rings own cloned runtime handles, clone on
  history reads, and release ring-owned handles on overwrite or scope cleanup.
  Bounded selector snapshots also feed iterator tails through the runtime-list
  path, while scalar latest selectors such as `@name[-1] >> ...` stay rejected
  as non-iterable inputs. Selectors that exceed the declared bound, use
  non-bounded resource shapes, or require unsupported tuple history storage
  remain fail-closed until their resource-family storage semantics are
  implemented.
- The explicit-copy selector slice is executable for those bounded resource
  families, including list/dict/matrix handle-valued snapshots: `snapshot <<
  @name[-n..]` and `snapshot << @name[...]` bind the materialized typed list
  snapshot to `snapshot`; `snapshot << @name[-1]` remains rejected because the
  latest read is scalar rather than an enumerable snapshot copy.
- The first type-directed materialized-container clone slice is executable:
  `copy << list_source`, `copy << dict_source`, and `copy << matrix_source`
  lower through list/dict/matrix clone IR and runtime helpers, produce
  independent owned containers, and `copy <- list_source` /
  `copy <- dict_source` / `copy <- matrix_source` now fail closed because
  `<-` is resource acquire or task pull, not bound-resource cloning. File,
  topology-resource, and broader family clone/copy semantics remain open.

Current RTG checks cover parts of resource topology, but the broader resource-management model is still design-level. The remaining gap is not "add another verifier." The gap is to define the facts that the existing compiler pipeline must enforce for resource values.

## Resource-Management Contract To Decide

### Resource Identity And Lifetime

Each resource subject needs a stable identity model:

- **External anchor:** a resource provided by a driver or environment, such as standard streams, files, sockets, exchanges, or host-provided handles.
- **Named language resource:** a top-level `@name` resource that stores, publishes, or snapshots values.
- **Materialized container:** a list, matrix, dictionary, range, or snapshot that behaves like a resource only for protocol/capability checks.
- **Hidden ledger:** compiler-owned state used by intrinsics such as moving averages.

The contract must say which operations each resource subject allows, which resources can be closed by user code, which resources can be cloned, and which values are views or snapshots rather than independent resources.

Accepted host-provided anchor decision:

- Host-provided resource anchors such as `@stdin`, `@stdout`, `@stderr`, `@file(...)`, sockets, exchange handles, and future driver handles may all be operated explicitly by user code.
- There is no borrow-only host-anchor category in Styio source.
- The allowed operations still come from the resource subject's declared capabilities. For example, `@stdin` can read/iterate and release/close through its resource operations; `@stdout` and `@stderr` can write and release/close; file-like resources can read, write, iterate, snapshot, and release/close when their declaration provides those capabilities.
- Data direction labels such as read-only or write-only describe value flow, not whether the resource can be explicitly released.

### Capability Vocabulary

Accepted v1 capability decision:

- The first public compiler-owned capability vocabulary is the complete baseline
  for the first implementation slice.
- Following the practical Rust trait / Go interface pattern, the public
  vocabulary stays small and operation-oriented; larger convenience concepts are
  predicates over these capabilities, not new source syntax.

| Capability | Meaning | Typical operations |
|------------|---------|--------------------|
| `pull` | Produces one item through an explicit receive/pull step | typed stdin pull, one-shot driver read |
| `iter` | Produces a sequence of items for a snapshot-backed block | `>>`, zip, stream iteration |
| `push` | Accepts items one by one | `expr -> @sink`, writer output |
| `index` | Supports indexed selection | `x[i]`, `@price[-1]` |
| `slice` | Supports range selection | `@price[-3..]`, `@price[...]` |
| `sized` | Exposes length or size facts | `.length`, `.size`, fixed windows |
| `collect` | Materializes streamed items | list/dict/snapshot collection |
| `clone` | Supports explicit deep copy into a fresh resource or container | `<<` when the source is cloneable |
| `close` | Has an explicit release protocol | `.close()`, scope-exit drop |

Derived concepts such as `Iterable[T]`, `Writable[T]`, `Indexable[T]`, and `Cloneable` should be predicates over this vocabulary, not separate parser forms or AST-node families.

### Typestate

Accepted typestate decision:

- Resource operations must be checked against protocol state.
- The initial stable typestate set is:

| State | Meaning |
|-------|---------|
| `uninit` | Declared but not initialized or acquired |
| `open` | Valid for normal resource operations |
| `eof` | Readable stream has ended; iterator progression may stop without failure |
| `closed` | Resource has been released; further consuming or I/O operations are invalid |
| `materialized` | Value is an in-memory collection or snapshot with stable contents |

`failed` is not a long-lived resource typestate. As in Rust-style `Result` and
Go-style explicit error returns, failure is a typed operation effect/result that
must be settled. The resource's post-failure protocol state is declared by the
operation family using the stable states above. A resource family cannot publish
an accepted operation unless it declares that post-effect state.

Examples:

- `open --pull--> open | eof`, with I/O, parse, or closed-handle problems reported as typed effects.
- `open --push--> open`, with pressure, I/O, or closed-handle problems reported as typed effects.
- `open --close--> closed`, with cleanup failure reported as `ResourceCleanupFailure` and post-failure state declared by the close family.
- `materialized --index--> materialized`, with bounds failure reported as a typed effect.

`eof` is not an error. It is iterator termination. I/O failure, parse failure,
bounds failure, cleanup failure, backpressure escalation, and closed-handle use
are effects/results, not implicit typestate fallthroughs. The current file
iterator implementation preserves that distinction for zero or invalid handles
by reporting the closed-handle runtime subcode before accepting EOF.

### Resource Access And Copy

Styio source does not expose `borrow`, `shared`, `own`, or `pure` as user syntax. A resource is the subject being operated on, not a reference that user code borrows from an owner.

- Each accepted operation must be declared on the resource subject's capability set.
- Resource sharing is not currently an accepted Styio source behavior.
- Block execution uses a snapshot instead of shared access to the original resource context.
- Explicit copy requires `clone`; clone is not implied by assignment.
- `clone` means deep copy: allocate independent storage/resource state, copy the reachable resource contents, and return a fresh owner.
- Styio does not use reference-counted clone semantics for resources; clone must not create another binding that shares the same mutable backing resource.
- `<<` remains an explicit feed/copy surface and must be type-directed, not parser-shape-directed.
- `<-` is not a clone surface for already-bound resources or materialized
  containers; it is reserved for resource-entry acquire/receive and task/future
  pulls.

The important decision is the resource rule visible to users and tests, not an imported borrow calculus from another language.

### Block-Entry Snapshot And Commit

Accepted block-entry decision:

- Every language form that enters a block creates a resource snapshot context at the block-entry operator.
- Covered block-entry operators include `>>`, `=>`, `?=`, the active task_launch operator `||>`, and the reserved `|>` family if it later becomes a block-entry surface.
- This rule covers resource state and resource effects. Ordinary lexical value scoping keeps its existing language rules.
- The block operates on the snapshot. It does not share mutable access to the original resource context while the block is running.
- The parameterized form `resource >> #(x) => { ... }` follows the same rule; the closure parameter is drawn from the snapshot stream.
- Match forms such as `x ?= { arm => { ... } }` enter a match snapshot context, and the selected arm block is its own block-entry stage.
- Task forms such as `||> { ... }` enter a task snapshot context when the task block is constructed; committing the task block result still follows task handle semantics.
- When the block reaches `}`, the compiler automatically commits the resulting snapshot/effects back to the source resource context for that stage.
- The block-end commit is the default commit barrier for the block. It may still be optimized internally, but the observable resource state after `}` must match the committed snapshot result.
- Chained block stages are multiple snapshot/commit units, not one transaction that waits for the final stage. A chain such as `a => { 1 } => { 2 } => { 3 }` creates three snapshots and performs three block-end commits.
- Each later block stage starts from the resource state committed by the previous stage.
- If the block exits through a fallible resource effect, the commit participates in the same typed resource-effect path as other resource operations.

### Resource Write Commit Boundary

Accepted `expr -> @name` decision:

- A resource write first creates a pending resource-write effect against the current resource context: the block-entry snapshot when inside a block, or the original resource when outside a block.
- The compiler should commit as late as correctness allows so Sema, lowering, optimization, and resource-topology reasoning can fuse, reorder, or remove writes that have no observable difference.
- Required commit barriers include same-resource reads that must observe the write, explicit snapshot/copy, iteration by another consumer, `flush`, `close`, resource-family release/commit hooks, and any explicit happens-before edge.
- Block-entering forms add one mandatory barrier: block exit commits the snapshot back to the source resource context for that stage.
- Resource families may define a later safe boundary such as scope exit or driver batch flush only when it does not violate the block-end commit rule.
- Failure ordering must remain typed: a delayed commit still carries the original write effect and any resource-family failure type.

This is an IM-D4 resource-effect rule. Cross-stream synchronization and global memory-model behavior remain IM-D5.

### Cleanup And Drop

Resource cleanup must be deterministic:

- Owned close-capable resources receive compiler-owned cleanup when the owner leaves scope.
- User code may call `.close()`, but safety must not depend on remembering to call it.
- Multiple resources in the same scope release in deterministic reverse-acquisition order unless topology dependencies require a stricter order.
- Reassignment of a flex binding that owns a resource must release the old occupant before overwriting the binding.
- Double-close and use-after-close must be either statically rejected or reported through the resource failure path.

Accepted cleanup-failure decision:

- Cleanup failure is a resource side effect, not an untyped runtime side channel.
- Cleanup failure has a distinct failure/effect type family: `ResourceCleanupFailure`.
- Type inference must carry cleanup effects for explicit `.close()`, implicit scope-exit drop, reassignment cleanup, and resource-family release/commit hooks.
- `?| resource_operation` settles at the current source site. If the resource operation fails and no fallback is present, the failure is raised immediately as a structured error instead of flowing downstream as a carried error value.
- Resource fallback uses the uniform `?| resource_operation | fallback` form. The fallback handles the `ResourceCleanupFailure` value/effect through normal type inference instead of hiding the failure.

The remaining IM-D4 work is to implement this decision across resource families.
The explicit file-write path now covers one cleanup-failure family:
`fclose` failure reports `STYIO_RUNTIME_FILE_CLEANUP_FAILURE`, matches the
`cleanup` handler family, and stays distinct from `io`. The file flex-rebind
slice also releases a tracked file handle before `name = @file(...)` evaluates
the RHS acquire or overwrites the owner, checks the cleanup error channel at that
rebind boundary, clears consumed-receiver state after a successful resource
rebind, and reopens a same-path singleton slot that an explicit close left at
zero. Explicit `?| name = @file(...) | fallback` / `| cleanup => handler`
settlement now reuses that rebind cleanup boundary: parser and Sema admit only
file-resource flex rebind, codegen branches a materialized cleanup failure to
`SIOResourceEffect` before replacement open, and file-open failures on the
replacement path recover through the existing `io` family. Tracked file handles are now also closed on explicit `<| return` before
the LLVM return, normal scope-pop cleanup checks for cleanup failures after the
cleanup boundary, and loop `^` / `>>` exits clean the tracked file scopes they
bypass before branching. The file
iterator closed-handle slice reports a structured `closed`-family diagnostic
when a stale same-path alias uses that zeroed slot. The file/stdin instant-pull
and materialized container-index/list-slice paths now cover the first value-producing
success/fallback/handler paths for non-task `?|` expressions, including
explicit-target stdin `f64`, `string`, typed-list values, list/dict/matrix
`bounds` recovery, ordered dict value-slice recovery through `d.values` plus
the list-slice bounds path, and returned matrix cell/row or row-range-slice bounds from
typed resource method parameters. Source-level fallback recovery for implicit
cleanup failures, non-file reassignment cleanup failures, release/commit hooks,
non-file resource-family cleanup effects, pressure-observer runtime streams,
using a handle acquired inside statement `?|` as a later
resource operation beyond the covered file iterator, close-method, and
write-method paths, and arbitrary value-producing resource operations
still require separate implementation and tests.

### Fallible Operations

Fallible resource operations should be typed internally even when Styio keeps a default fail-fast surface.

| Operation family | Success value | Failure family |
|------------------|---------------|----------------|
| pull/read | item or typed value | I/O error, parse error, closed handle |
| iter step | `Yield(T)` or `End` | I/O error, parse error, closed handle |
| push/write | unit | I/O error, closed handle, escalated `ResourceBackpressureFailure` |
| index/slice | item or snapshot | bounds error, invalid selector |
| clone/copy | fresh owned resource or snapshot | unsupported clone, allocation failure |
| close/drop | unit | `ResourceCleanupFailure` |

The design should preserve the distinction between iterator `End` and operation `Err`. `End` is normal control flow; `Err` enters failure handling.

### Default Failure Handling

Styio should not require explicit user-level unwrap at every resource operation.

The default contract should be:

- fallible operations are represented internally as `Result[T, E]` or an equivalent typed effect,
- ordinary expression, statement, and explicit `?| resource_operation` boundaries settle the result,
- `Err` at a boundary without fallback aborts the current execution path with a structured diagnostic,
- `?| resource_operation | fallback` may recover from `ResourceCleanupFailure` or another resource effect by evaluating a fallback expression or block,
- fallback recovery participates in type inference: the successful operation value and fallback value must unify with the surrounding use-site type.

This keeps failure visible to the compiler without turning every simple resource program into manual error plumbing.

### Resource Effect Evaluation And Fallback

Accepted resource fallback decision:

- `?|` is the uniform source marker for resource effect evaluation.
- `?| resource_operation` evaluates and settles a resource operation through the typed resource-effect path. Without a fallback, failure is raised immediately at that site.
- `?| resource_operation | fallback` evaluates `fallback` only when the resource operation yields a resource effect failure, then type-checks the recovered value against the successful operation value and surrounding use-site type.
- `?| resource_operation | effect_name => handler` is the effect-specific handler form. It handles only the named typed effect family, such as `backpressure`, and the handler result must still type-check against the operation's success type and surrounding use site.
- `?| resource_operation | e1 => handler1 | e2 => handler2` chains effect-specific handlers. The first matching typed effect family runs; unmatched failure effects use the default fail-fast rule unless a final catch-all fallback is present.
- A bare `| fallback` is not a resource fallback form. Bare `|` remains available for guard else branches and ordinary value-level fallback where those grammars already own it.
- `?| resource_operation | ...` is an audited resource-effect discard statement. It is allowed only as a standalone statement, never as an expression or value-producing form. It means: execute and settle the resource operation; if resource effects arise, discard business recovery for this site; then continue with the next statement.
- A discard statement does not pretend the operation succeeded, does not produce a value, does not skip resource-state settlement, and does not bypass cleanup, commit, diagnostic, trace, or pressure accounting required by the resource family.
- `?| resource_operation | effect => @()` is also not accepted as a "do nothing" handler. `@()` is the empty resource / destroy sink, not an executable empty action.
- Await keeps the same marker because task/future await is also a resource-like effect: `?| task -> value: T` raises immediately on failed pull, while `?| task -> value: T | fallback` recovers through the same typed fallback path.
- `?=` remains ordinary value/pattern matching. A form such as `res_op ?= { backpressure => { ... } }` only applies after `res_op` has explicitly materialized a normal discriminated value or result; it does not implicitly catch resource effects.
- Future syntax may add more ergonomic forms, but the current uniform resource fallback surface is `?| ... | ...`.

Implementation note: the current value-producing non-task slices are limited to
file instant pulls, stdin instant pulls, materialized container index/row
reads, materialized list slices, and user-defined resource methods whose body is
a single `<| expr` return. Returned match expressions preserve the current
scalar/string match result families, while returned container match results
remain fail-closed. File
instant pulls still return `i64`; stdin instant pulls now cover the untyped
`i64` path plus explicit-target `f64`, `string`, and supported typed-list paths
under `?|` expression recovery. Materialized list, dict, and matrix indexing
recovers `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, and
`STYIO_RUNTIME_MATRIX_INDEX` through catch-all fallback or matched `bounds`
handlers; matrix row and row-range slice recovery use the same matrix bounds
family, with row ranges lowering through `SCMatrixRowsSlice` /
`styio_matrix_rows_slice_i64` or `styio_matrix_rows_slice_f64`, and list slice
recovery uses `SCListSlice` / `styio_list_slice` with the list bounds family.
Single-return resource methods may return calls to ordinary block-form
functions whose body has a final value tail or explicit `<| expr`; direct calls
and `?| method() | fallback` preserve the called function result family, while
statement-only called functions remain fail-closed as method return values.
Resource method/property bodies may use receiver-scoped property access such as
`@file.path`, including property bindings like `@file::self_path := @file.path`
and returned format strings such as `<| $"path={@file.path}"`; the same
`@file.path` spelling outside a resource-family definition remains fail-closed.
Single-return resource methods may also return a file instant pull and recover that
returned `STYIO_RUNTIME_FILE_OPEN_READ` through fallback or matched `io`
handlers under `?|`; they may also return canonical `(<- @stdin)` and recover
returned `STYIO_RUNTIME_NUMERIC_PARSE` through fallback or matched `parse`
handlers under `?|`, or return materialized list index/list-slice, inline
dict-index, ordered dict value-slice, or typed-parameter matrix cell/row or row-range slice expressions
that recover
returned `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, or
`STYIO_RUNTIME_MATRIX_INDEX` through fallback or matched `bounds` handlers
under `?|`. Declared resource method parameter types are bound while inferring
the method body and are checked at call sites; unimplemented lexical/global
capture still fails closed before lowering.
Statement-shaped writes, file acquire, file rebind, file close methods, and guarded
acquired-handle file write methods have the first statement settlement paths;
file acquire also covers successful acquire followed by file iteration,
successful acquire followed by a later resource-effect close method, successful
acquire followed by a later guarded write method, close-method receiver
invalidation, and fail-closed iterator or write-method use after a recovered
failed acquire. File rebind covers catch-all fallback and matched `io` handler
recovery for replacement open failures, keeps scalar flex binds fail-closed, and
keeps value-required rebind expressions rejected; the codegen cleanup branch
routes prior-handle cleanup failure to the wrapper before opening a replacement.
Broader resource families, cleanup/drop hooks,
pressure-observer payloads and runtime execution, failing value-producing resource methods beyond returned
file/stdin instant pulls, returned block-form function calls, and returned list/dict/matrix bounds slices,
multi-statement value-producing resource methods, resource-method lexical/global
captures, and other non-instant-pull
value-returning resource operations must remain separately implemented and
tested before the full typed resource-effect model is closed.

Allowed fallback installation points:

1. Explicit resource-effect settlement: `?| resource_operation | fallback`.
2. Task/future pull settlement: `?| task -> value: T | fallback`.
3. Effect-specific resource settlement: `?| resource_operation | effect_name => handler`.
4. Effect-handler chains: `?| resource_operation | e1 => handler1 | e2 => handler2`.
5. Audited statement-only effect discard: `?| resource_operation | ...`.
6. Future resource block-entry settlement, if a block-entry form supports recovery, must use the same `?| block_entry_operation | fallback`, `?| block_entry_operation | effect_name => handler`, handler-chain, or statement-only discard shape. A trailing `} | fallback` is not resource fallback.

All other fallible resource operations still settle at their ordinary statement,
expression, commit, or cleanup boundary. If no explicit `?| ... | fallback`,
named handler, or statement-only discard is present at that boundary, failure is
raised immediately and is not carried into later inference as a recoverable
value.

### Backpressure

Accepted backpressure decision:

- Backpressure on a single `push` or `write` operation is first a typed resource
  pressure signal, not an error: `ResourceBackpressure`.
- A write that is pressured may wait, remain pending, or be scheduled according
  to the resource family's policy. This is not program failure by itself.
- If the resource policy decides that pressure has become unrecoverable, such as
  a closed channel, failed transport, timeout, or exceeded backlog limit, the
  pressure signal may escalate to `ResourceBackpressureFailure`.
- `?| write_operation | fallback` may recover from the escalated failure through
  the same resource fallback path as other resource effects.
- `?| write_operation | backpressure => handler` is valid when the resource
  family exposes backpressure as a typed pressure effect at the settlement site.
  The handler can perform useful side effects such as counting, logging, starting
  inspection, or attempting recovery while the write remains governed by the
  resource family's pressure policy.
- A pressure handler must be executable resource code. `backpressure => @()` is
  rejected because `@()` is an empty resource sink, not an action. `backpressure
  => ...` is also rejected because ellipsis is not a handler body.
- `?| write_operation | ...` is valid only as a standalone statement. It discards
  business handling for pressure or later failure at that site, while the resource
  family still accounts for pending writes, pressure state, backlog, escalation,
  diagnostics, and cleanup normally.
- `?=` is not the default backpressure handler. It may match a backpressure case
  only when an earlier operation has explicitly produced a normal value such as a
  result object; it does not intercept unsettled resource effects.
- A resource family may expose pressure as an explicit observable effect stream,
  such as `channel.pressure >> #(p) => { ... }`, where `p` contains facts such as
  pending count, high-water mark, operation family, and resource state.
- Pressure observers are ordinary side-effecting resource code. They can count,
  log, spawn a task, or call a recovery operation, but their own resource actions
  still use the same capability, snapshot/commit, and `?| ... | fallback` rules.
- The current compiler recognizes `resource.pressure >> #(p) => { ... }`
  syntax and then rejects every current resource family in Sema with
  `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`. This is an
  unsupported-family boundary only; it does not define the pressure payload,
  schedule observer execution, or add a runtime pressure stream.
- Waiting, retry scheduling, fairness between streams, and producer throttling
  policies are IM-D5 stream/concurrency concerns, but IM-D4 owns the resource
  contract that makes pressure observable without pretending it is always a
  failure.

Illustrative recovery pattern:

```styio
channel.pressure >> #(p) => {
    ?(p.pending > 10000) => {
        ||> {
            ?| channel.inspect()
              | inspection_failed => report_pressure_probe(p)
        }
    }
}
```

This pattern is intentionally explicit. The language permits it for resource
owners who want local side-effect recovery logic, while production deployments
may still prefer external process supervision.

Design rationale:

- This is regular Styio resource design, not an exceptional convenience feature.
  A resource operation can produce useful typed effects that are neither success
  values nor immediate failures.
- The compiler is expected to perform the extra effect inference needed to keep
  those cases distinct when the distinction buys a real language capability.
- Backpressure is the motivating example: most pressure is harmless and merely
  means the resource is waiting. Treating every pressure event as failure would
  erase the useful state that lets user code count, inspect, and trigger a
  side-effecting recovery path.
- `?| ... | fallback` remains the universal recovery surface for failures, but
  observable pressure is a first-class resource effect before it escalates into
  that failure path.
- Styio intentionally rejects implicit "do nothing" handler bodies for resource
  effects. A handler body must be an executable expression or block, and `@()` is
  not an executable empty action. The only accepted do-no-business-recovery form
  is the audited statement discard `?| op | ...`, which settles the resource
  operation and then continues without producing a value.

## Boundaries

IM-D4 explicitly does not own these already-separated maturity items:

| Boundary | Owner |
|----------|-------|
| StyioIR verifier placement, `SGNoOp`, active AST classification, codegen accepting only verified IR | IM-D1 |
| Accepted grammar authority and parser fallback rejection | IM-D2 |
| Public diagnostic code naming and JSON/LSP diagnostic envelopes | IM-D3 |
| Cross-stream synchronization, pulse-frame locking, and memory model | IM-D5 |
| Native interop ABI, target triples, symbol manifests, and external C/C++ build contracts | IM-D7 |
| Package lifecycle, registry trust, lockfiles, and vendoring UX | IM-D10 |

## Stop Condition

IM-D4 can close only when every accepted resource operation is covered by:

1. a capability requirement,
2. a resource-subject access rule,
3. a typestate precondition and transition,
4. a snapshot and commit rule for every block-entry operator,
5. a commit-barrier rule for pending resource writes,
6. a cleanup rule when the operation creates or releases a resource,
7. a failure result/effect rule, including `ResourceCleanupFailure` where cleanup can fail,
   a fallback rule when the source form permits `?| op | fallback`,
   and a statement-only discard rule when the source form permits `?| op | ...`,
8. positive tests for accepted behavior, and
9. negative tests for invalid state, invalid capability, retired syntax, and unsupported resource families.

Until then, unsupported or retired resource forms must remain rejected by named migration diagnostics instead of falling through to placeholder lowering or runtime guesses.

## Decision Closure

No IM-D4 language-design decision remains open in this inventory. Remaining work
is implementation and tests: every accepted resource family must encode the v1
capability requirement, stable typestate transition, post-effect state, fallback
or discard behavior, cleanup rule, and negative diagnostics described above.

## Source Documents

- [Styio Resource Topology](../design/Styio-Resource-Topology.md)
- [Styio Handle, Capability, and Failure Type System](../design/Styio-Handle-Capability-Type-System.md)
- [Styio EBNF Appendix B: Topology v2](../design/Styio-EBNF.md#appendix-b-topology-v2--resource-declarations)
- [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md)
