# IM-D4 Resource Management Inventory

**Purpose:** Record the accepted resource-management decisions for IM-D4 without duplicating the IM-D1 StyioIR verifier, lowering, no-op, or codegen-gate contract.

**Last updated:** 2026-05-26

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
  preserves task_await binding for `?| task -> value: T`.
- Bounded `i64`, `f64`, `bool`, `char`, and `string` resource selectors now have distinct
  executable value shapes: `@name[-n]` reads a scalar value, while
  `@name[-n..]` and `@name[...]` materialize typed list snapshots from explicit
  history reads. Char rings store `i8` values and materialized `list[char]`
  snapshots render escaped char literals; string selector rings own cloned cstr
  values and release them on overwrite or scope cleanup. Selectors that exceed
  the declared bound, use non-bounded resource shapes, or require unsupported
  tuple/list/dict/matrix history storage remain fail-closed until their
  resource-family storage semantics are implemented.
- The first explicit-copy selector slice is executable for those bounded
  resource families: `snapshot << @name[-n..]` and `snapshot << @name[...]`
  bind the materialized typed list snapshot to `snapshot`; `snapshot <<
  @name[-1]` remains rejected because the latest read is scalar rather than an
  enumerable snapshot copy. Full type-directed `<<` clone/copy semantics remain
  open for broader resource families.

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
are effects/results, not implicit typestate fallthroughs.

### Resource Access And Copy

Styio source does not expose `borrow`, `shared`, `own`, or `pure` as user syntax. A resource is the subject being operated on, not a reference that user code borrows from an owner.

- Each accepted operation must be declared on the resource subject's capability set.
- Resource sharing is not currently an accepted Styio source behavior.
- Block execution uses a snapshot instead of shared access to the original resource context.
- Explicit copy requires `clone`; clone is not implied by assignment.
- `clone` means deep copy: allocate independent storage/resource state, copy the reachable resource contents, and return a fresh owner.
- Styio does not use reference-counted clone semantics for resources; clone must not create another binding that shares the same mutable backing resource.
- `<<` remains an explicit feed/copy surface and must be type-directed, not parser-shape-directed.

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
`cleanup` handler family, and stays distinct from `io`. Implicit scope-exit
drop, reassignment cleanup, release/commit hooks, and non-file resource-family
cleanup effects still require separate implementation and tests.

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
