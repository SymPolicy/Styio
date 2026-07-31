# Active Syntax Map

**Purpose:** Provide a compact composed authoring map for current Styio syntax; feature-level authority lives in [features/](./features/), while shared grammar, token, and semantic invariants stay in the cross-feature design documents.

**Last updated:** 2026-07-31

## Reading Contract

1. This page lists canonical authoring forms and the active implementation-facing compatibility surface; it is a composed view, not a feature SSOT.
2. It is not a catalog of retired syntax. Historical spellings are recovered from Git history only when needed for migration archaeology.
3. Positive examples in `example/` and `tests/` should use the canonical forms below unless a test is explicitly a compatibility or negative migration fixture.
4. Start lifecycle, dependency, prerequisite, or evidence changes in the owning [feature SSOT](./features/README.md), then regenerate this collection's indexes and feature graph.

## Core Forms

| Area | Canonical form | Owner |
|------|----------------|-------|
| Imports | `@import { pkg/module }` | Direct executable callable imports resolve relative to the importing source and require a fresh sibling `.styioi`; [../Styio-EBNF.md](../Styio-EBNF.md) |
| Final binding | `name := expr` | [../Styio-Language-Design.md](../Styio-Language-Design.md) |
| Mutable binding | `name = expr` | [../Styio-Language-Design.md](../Styio-Language-Design.md) |
| Callable binding | `# identity := (value) => value`, `# name : i64 := (arg: i64) => { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Match sugar | `#(name = expr) ?= { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Return/export | `<| expr` | [CONTINUATION_TRANSFER.md](./CONTINUATION_TRANSFER.md) |
| Inline return | `|<| expr |;` | [CONTINUATION_TRANSFER.md](./CONTINUATION_TRANSFER.md) |
| Conditional | `?(cond) => { ... } | { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Materialized range | `[start..end]` | Canonical iterable/source form. The bracketed form materializes the expression-level range `start..end` as `list[i64]`; it is not a list literal containing one range expression. `[start..end] >> #(x) => { ... }` advances the range one item at a time and pushes each item into the consumer. Step ranges are reserved and not active syntax. |
| Iteration / pulse transfer | `iterable >> #(x) => { ... }` | `>>` advances the left iterable or stream one item at a time and pushes each item as a pulse into the right closure/channel. |
| Infinite stream loop | `[...] >> ?(cond) => { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Continue | `>>`, `>>>`, `>>>>`, ... as a standalone statement | Skip the rest of the current block for this pulse/session and resume at the next pulse/session of the nearest continue-capable domain; token length is ignored. |

### Binding Model

`=` is the mutable binding operator. It may bind or rebind an ordinary value with
`name = expr`, and it may bind or rebind a callable or operation-channel endpoint
with `# name = (...) => expr` or `# name = #(args) => expr`.

`:=` is the final binding operator. It may bind an ordinary final value with
`name := expr`, and it may bind a final callable or operation-channel endpoint
with `# name := (...) => expr` or `# name := #(args) => expr`. A final callable
binding cannot later be replaced by `=` or another `:=`.

`#` marks a binding as callable/operable; it is not a resource prefix and not a
plain `def` keyword. The right side of a `#` binding must be a callable body or
operation-channel body. Resource identities remain visibly in the `@` family, so
`# sink = @stdout` is invalid; use `expr -> @stdout`, `items >> @stdout`, or a
resource-family declaration when the target is a resource.

Callable generics are inference-owned. Authors never place a generic parameter
list after a callable name: `# name[T] ...` is invalid. Eligible final,
non-recursive definitions receive one stable principal rank-1 relation at the
definition site; the compiler may publish that relation as module-interface
metadata and instantiate it freshly at each use.

A recursive call-graph component is solved as one group with one provisional
monotype per member. Internal recursive references must reuse the same
provisional type variables. After a stable solution, eligible final bindings
may be generalized and published, so ordinary generic recursion remains
available. A recursive edge that needs the same member at a different
instantiation is rejected as polymorphic recursion.

Generalization also requires a closed, proven-pure callable body. Output,
resource, task, handler, native, captured-environment, and unknown effects
propagate through direct calls. A callable with any such summary remains
monomorphic; a later use at a conflicting concrete type is rejected before
lowering. Effect-row source syntax and purity annotations are not active.

Type-variable names shown by the compiler or an IDE are explanatory metadata,
not source declarations. Concrete annotations remain available for monomorphic
contracts, and `: T` always refers to an already defined type named `T`.

Callable uses are instantiated only from ordinary arguments and the concrete
expected type supplied by their surrounding context. Call-site type arguments
do not exist: write `identity(1)`, not `identity[i64](1)`. In value position,
`[]` remains an ordinary selector/index and is never reinterpreted as generic
specialization. If a call remains underconstrained, add a concrete annotation
to its surrounding binding or expression; do not add a callable type-argument
list.

Operator-bearing inferred callables carry compiler-owned constraints. The
closed vocabulary is numeric, comparable, indexable, iterable, and cloneable;
there is no source constraint or instance syntax. Arithmetic, comparison, and
index expressions currently provide executable constraint evidence, and an
unsatisfied concrete instance is rejected before lowering.

Unannotated numeric literals normalize to `i64` or `f64`. An unresolved
numeric-only relation variable may default to `i64` only after relation and
constraint solving. Empty `[]` and `dict {}` never receive a fabricated element
or value type: supply a surrounding `list[T]` or `dict[K,V]` annotation.

An inferred scheme remains attached to direct named calls only. `identity(1)`
may instantiate a fresh relation, but a bare `identity` used as a stored,
passed, returned, collected, or captured value is rejected in Sema. Such a
value position must first provide one concrete monomorphic callable type; the
current grammar and StyioIR expose no typed callable-value boundary, so they do
not silently lower a scheme to an untyped function pointer.

Generalized relation variables use a closed plain-value domain: immutable
scalars and recursively plain materialized `list`/`dict` types. Resource,
stream, file, task, matrix, topology-resource, range-handle, user-defined, and
other capability-sensitive types remain monomorphic. The check precedes
relation normalization, so a resource-shaped sequence cannot masquerade as an
ordinary list and nested handle elements remain visible.

Callable interfaces are compiler-owned artifacts, not authored syntax. Publish
one explicitly with `--module-id` and `--emit-module-interface`, then import its
source path normally. The loader accepts only matching schema, compiler/target
ABI, source, checked-body, and direct-dependency digests. Consumers see only
exports from direct imports; private helpers remain scoped to their owning
imported body, transitive names do not leak, and module dependency cycles fail
closed before Sema.

Concrete inferred-callable code is demand-driven. Ordinary direct calls form a
reachable mono-item graph; repeated equal instances reuse one deterministic
content-addressed definition, while unreachable generic definitions and
unreachable imported concrete helpers emit no code. Identity includes the
canonical relation, effects, checked body, transitive callable dependencies,
module facts, target, and backend ABI, so call order does not affect symbols
and a reachable callee-body change invalidates callers.

There is no explicit-instantiation syntax. Exact same-instance recursion reuses
the active item; expansion beyond 64 active instances or compilation growth
beyond 4,096 items fails with a concrete instance path. These hard safety
ceilings add no boxing, witness tables, runtime dictionaries, generic heap, or
GC.

## Types

| Shape | Meaning |
|-------|---------|
| `T|n|` | exact-length resource shape |
| `T|..n|` | recent-window resource shape |
| `T..` / `T...` | unbounded sequence type |
| `list[T]` | type rewrite to `T..` |
| `dict[K, V]` | type rewrite to `(K, V)..` |
| `(A, B, C)` | tuple type |
| `__ : Pattern := Type` | type rewrite declaration in type position |

## Resources

| Area | Canonical form | Notes |
|------|----------------|-------|
| Resource slot | `@price : f64|..10|` | Top-level only. |
| Multi-resource slot | `@a : f64|..2|, @b : f64|..2| := { ... }` | Driver-block coverage remains staged by topology tests. |
| Sink write | `expr -> @price` | Produces a pending write against the current resource context. |
| Resource block | `resource >> { ... }` / `resource >> #(x) => { ... }` | Iterates the resource one produced item at a time into the block, enters a snapshot at `>>`, and commits snapshot result at `}`. Chained block stages commit once per block. |
| Latest read | `@price[-1]` | Reads committed resource state or the current block snapshot. |
| Slice read | `@price[-3..]`, `@price[...]` | Resource-object selectors; bounded selector snapshots materialize iterable lists, while scalar latest reads remain non-iterable. |
| Stdin pull | `value <- @stdin` | Untyped scalar pull. |
| Typed stdin pull | `a, b <- @stdin : (f64, f64)` | Tuple/list/scalar forms share the stdin-pull path. |
| Stdin iteration | `@stdin >> #(line) => { ... }` | `@stdin` is read-only for data flow; each input line is pushed as one pulse into `line`; explicit resource operations may still release it. |
| Stdin zip source | `@stdin >> #(line) & xs >> #(x) => { ... }` | Finite zip is accepted with materialized lists or `@file` streams; duplicate `@stdin & @stdin` remains unsupported until the stream-driver contract defines duplicate consumption. |
| Stdout/stderr scalar write | `expr -> @stdout`, `expr -> @stderr` | `@stdout` and `@stderr` are write-only data sinks; explicit resource operations may still release them. |
| Resource-sink iterable write | `items >> @stdout`, `items >> @stderr`, `items >> @file("out.txt")` | Each iterable item is serialized and written as a sink pulse. Plain strings should use `->` unless explicitly split. |
| File resource | `@("log.txt")`, `@file("log.txt")` | Runtime substrate is file-backed when resolved as a file. |
| Empty resource sink | `@()` | Destroy sink / empty resource. |
| Explicit copy | `snapshot << @price[...]`, `copy << list_or_dict_or_matrix` | Selector snapshots and materialized list/dict/matrix handles copy through `<<`; `<-` stays resource acquire / task pull, not bound-resource clone. |
| Resource effect fallback | `?\| resource_operation \| fallback` | `?\| resource_operation` settles in place and raises immediately on failure; fallback participates in type inference. Statement resource operations include file acquire/rebind forms such as `?\| f <- @file("log.txt") \| fallback` and `?\| f = @file("next.log") \| fallback`, direct file iterators such as `?\| @file("data.txt") >> #(line) => { ... } \| fallback`, direct file releases such as `?\| @file("log.txt") -> @() \| fallback`, plus resource method calls such as `?\| @file("log.txt").close() \| fallback`; value-producing forms include stdin pulls with explicit target types, such as `result: f64 = ?\| (<- @stdin) \| fallback`, file instant pulls such as `value = ?\| (<< @file("data.txt")) \| fallback`, acquired file-handle pulls after `?\| f <- @file(...) \| ...` such as `value = ?\| (<< f) \| fallback`, materialized container bounds reads such as `value = ?\| items[i] \| fallback`, `slice = ?\| items[0..] \| fallback`, `value = ?\| table[key] \| fallback`, ordered dict value slices such as `values = ?\| table[start..end] \| fallback`, `value = ?\| matrix[row][col] \| fallback`, `row = ?\| matrix[row] \| fallback`, or `rows = ?\| matrix[start..end] \| fallback`, and value-returning resource method calls such as `value = ?\| log.answer() \| fallback`, including methods whose body is a single returned expression, a statement-only preface plus a final returned expression, or local list/dict/matrix prefaces that return scalar/string values or the local container value. Bare `\| fallback` is not resource fallback. |
| Effect-specific handler | `?\| res -> msg_queue \| backpressure => do_something()` | Handles only the named typed effect family. Handler chains are allowed. |
| Effect discard statement | `?\| res -> msg_queue \| ...` | Standalone statement only. Settles the operation, discards business recovery, produces no value, and continues with the next statement. |
| Pressure observer | `channel.pressure >> #(p) => { ... }` | Optional resource-family effect stream for observing non-failure pressure such as backlog growth. The current compiler recognizes this syntax but no current resource family exposes a pressure stream yet; unsupported families fail closed in Sema with `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`. Observer actions still use normal resource rules when a future family implements the stream. |

## Resource Family Members

| Area | Canonical form | Notes |
|------|----------------|-------|
| Mutable method binding | `@file::close = () => { @file -> @() }` | Standard-library bindings may be overridden. |
| Final method binding | `@file::close := () => { @file -> @() }` | Final binding cannot be overridden. |
| Property binding | `@file::path := expr` | Property access is not callable. |
| Method call | `log.close()` | Consuming methods invalidate the receiver immediately. |
| Direct resource call | `@("log.txt").close()` | Late resource construction is still statically resolved before lowering. |
| Receiver reference | `@file` / `@file.path` inside `@file::name` body | Refers to the receiver instance, not a constructor; receiver postfix property/method access is scoped to the resource-family definition body. |
| Statement-preface return | `@file::answer = () => { >_("inside") <\| 42 }` | Value-returning method bodies may run accepted statement-only prefaces before a final returned expression. Scalar local `=` or `:=` prefaces such as `x = 41; <\| x + 1`, plus local list/dict/matrix `=` or `:=` prefaces such as `xs = [40, 2]; <\| xs`, `m: matrix := [[1, 2]]; <\| m[0][0]`, or `m: matrix := [[1, 2]]; <\| m`, are scoped to the inlined method body when the final return is scalar/string or the local container value; local resource bindings and capture-dependent bodies remain fail-closed. |
| Returned resource effect | `@file::read_or = () => { <\| ?\| (<< @file("data.txt")) \| 7 }` | Single-return method bodies may return a value-producing resource-effect expression; discard `?\| op \| ...` remains statement-only and is rejected as a returned expression. |
| Returned match expression | `@file::pick = (x: int) => { <\| x ?= { 0 => 'a' _ => 'b' } }` | Single-return method bodies may return match expressions for the current scalar/string result families; container match results remain fail-closed. |
| Returned function call | `@file::score = (x: i64) => { <\| plus_one(x) }` | Single-return method bodies may return calls to ordinary functions when the called function has a value tail or explicit `<\| expr`; statement-only function bodies remain fail-closed as method return values. |

## Tasks

| Area | Canonical form | Notes |
|------|----------------|-------|
| Single task | `job = ||> { ... }` | Produces a task handle. |
| Task group | `||> [ t1 := { ... } t2 := { ... } ]` | Each entry binds a task handle. |
| Await | `?\| job -> value: T \| fallback` | Uses the same effect-evaluation marker; without fallback, failed pull raises immediately. |
| Resource order | `t1 => t2` | Explicit happens-before edge; it does not transfer data. |

## Rejected Families

The active syntax docs name rejected families only as migration boundaries:

1. Retired state-resource containers and state references are parser errors.
2. Source-level bare `@` is not an authoring form.
3. Reserved wave tokens `<~` and `~>` remain fail-closed.
4. Compatibility spellings that remain implemented must not be used as the default teaching surface.
