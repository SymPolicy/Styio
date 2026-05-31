# Active Syntax Map

**Purpose:** Provide the compact authoring map for current Styio syntax; grammar authority stays in [../Styio-EBNF.md](../Styio-EBNF.md), token authority stays in [../Styio-Symbol-Reference.md](../Styio-Symbol-Reference.md), and semantics stay in the owning design documents.

**Last updated:** 2026-05-31

## Reading Contract

1. This page lists canonical authoring forms and the active implementation-facing compatibility surface.
2. It is not a catalog of retired syntax. Historical spellings are recovered from Git history only when needed for migration archaeology.
3. Positive examples in `example/` and `tests/` should use the canonical forms below unless a test is explicitly a compatibility or negative migration fixture.

## Core Forms

| Area | Canonical form | Owner |
|------|----------------|-------|
| Imports | `@import { pkg/module }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Final binding | `name := expr` | [../Styio-Language-Design.md](../Styio-Language-Design.md) |
| Mutable binding | `name = expr` | [../Styio-Language-Design.md](../Styio-Language-Design.md) |
| Function | `# name : T := (arg: U) => { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Match sugar | `#(name = expr) ?= { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Return/export | `<| expr` | [CONTINUATION_TRANSFER.md](./CONTINUATION_TRANSFER.md) |
| Inline return | `|<| expr |;` | [CONTINUATION_TRANSFER.md](./CONTINUATION_TRANSFER.md) |
| Conditional | `?(cond) => { ... } | { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Range literal | `[start..end]`, `[start..end..step]` | Integer expressions only; materializes `list[i64]` for expression/value use and remains iterable in `>> #(x)` loops. |
| Infinite stream loop | `[...] >> ?(cond) => { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |

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
| Resource block | `resource >> { ... }` / `resource >> #(x) => { ... }` | Enters a snapshot at `>>`; commits snapshot result at `}`. Chained block stages commit once per block. |
| Latest read | `@price[-1]` | Reads committed resource state or the current block snapshot. |
| Slice read | `@price[-3..]`, `@price[...]` | Resource-object selectors; bounded selector snapshots materialize iterable lists, while scalar latest reads remain non-iterable. |
| Stdin pull | `value <- @stdin` | Untyped scalar pull. |
| Typed stdin pull | `a, b <- @stdin : (f64, f64)` | Tuple/list/scalar forms share the stdin-pull path. |
| Stdin iteration | `@stdin >> #(line) => { ... }` | `@stdin` is read-only for data flow; explicit resource operations may still release it. |
| Stdin zip source | `@stdin >> #(line) & xs >> #(x) => { ... }` | Finite zip is accepted with materialized lists or `@file` streams; duplicate `@stdin & @stdin` remains unsupported until the stream-driver contract defines duplicate consumption. |
| Stdout/stderr scalar write | `expr -> @stdout`, `expr -> @stderr` | `@stdout` and `@stderr` are write-only data sinks; explicit resource operations may still release them. |
| Stdout/stderr iterable write | `items >> @stdout`, `items >> @stderr` | Plain strings should use `->` unless explicitly split. |
| File resource | `@("log.txt")`, `@file("log.txt")` | Runtime substrate is file-backed when resolved as a file. |
| Empty resource sink | `@()` | Destroy sink / empty resource. |
| Explicit copy | `snapshot << @price[...]`, `copy << list_or_dict_or_matrix` | Selector snapshots and materialized list/dict/matrix handles copy through `<<`; `<-` stays resource acquire / task pull, not bound-resource clone. |
| Resource effect fallback | `?\| resource_operation \| fallback` | `?\| resource_operation` settles in place and raises immediately on failure; fallback participates in type inference. Statement resource operations include file acquire/rebind forms such as `?\| f <- @file("log.txt") \| fallback` and `?\| f = @file("next.log") \| fallback`, direct file iterators such as `?\| @file("data.txt") >> #(line) => { ... } \| fallback`, direct file releases such as `?\| @file("log.txt") -> @() \| fallback`, plus resource method calls such as `?\| @file("log.txt").close() \| fallback`; value-producing forms include stdin pulls with explicit target types, such as `result: f64 = ?\| (<- @stdin) \| fallback`, file instant pulls such as `value = ?\| (<< @file("data.txt")) \| fallback`, acquired file-handle pulls after `?\| f <- @file(...) \| ...` such as `value = ?\| (<< f) \| fallback`, materialized container bounds reads such as `value = ?\| items[i] \| fallback`, `slice = ?\| items[0..] \| fallback`, `value = ?\| table[key] \| fallback`, ordered dict value slices such as `values = ?\| table[start..end] \| fallback`, `value = ?\| matrix[row][col] \| fallback`, `row = ?\| matrix[row] \| fallback`, or `rows = ?\| matrix[start..end] \| fallback`, and value-returning resource method calls such as `value = ?\| log.answer() \| fallback`, including methods whose body is a single returned expression or statement-only preface plus a final returned expression. Bare `\| fallback` is not resource fallback. |
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
| Statement-preface return | `@file::answer = () => { >_("inside") <\| 42 }` | Value-returning method bodies may run accepted statement-only prefaces before a final returned expression. Scalar local `=` or `:=` prefaces such as `x = 41; <\| x + 1` or `x := 41; <\| x + 1` are scoped to the inlined method body; local container/resource bindings and capture-dependent bodies remain fail-closed. |
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
