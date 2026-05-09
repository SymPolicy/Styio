# Active Syntax Map

**Purpose:** Provide the compact authoring map for current Styio syntax; grammar authority stays in [../Styio-EBNF.md](../Styio-EBNF.md), token authority stays in [../Styio-Symbol-Reference.md](../Styio-Symbol-Reference.md), and semantics stay in the owning design documents.

**Last updated:** 2026-05-09

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
| Sink write | `expr -> @price` | Produces a pending resource write. |
| Latest read | `@price[-1]` | Reads committed snapshot state. |
| Slice read | `@price[-3..]`, `@price[...]` | Resource-object selectors. |
| Stdin pull | `value <- @stdin` | Untyped scalar pull. |
| Typed stdin pull | `a, b <- @stdin : (f64, f64)` | Tuple/list/scalar forms share the stdin-pull path. |
| Stdin iteration | `@stdin >> #(line) => { ... }` | `@stdin` is read-only. |
| Stdout/stderr scalar write | `expr -> @stdout`, `expr -> @stderr` | `@stdout` and `@stderr` are write-only sinks. |
| Stdout/stderr iterable write | `items >> @stdout`, `items >> @stderr` | Plain strings should use `->` unless explicitly split. |
| File resource | `@("log.txt")`, `@file("log.txt")` | Runtime substrate is file-backed when resolved as a file. |
| Empty resource sink | `@()` | Destroy sink / empty resource. |
| Explicit copy | `snapshot << @price[...]` | Only explicit copy entry. |

## Resource Family Members

| Area | Canonical form | Notes |
|------|----------------|-------|
| Mutable method binding | `@file::close = () => { @file -> @() }` | Standard-library bindings may be overridden. |
| Final method binding | `@file::close := () => { @file -> @() }` | Final binding cannot be overridden. |
| Property binding | `@file::path := expr` | Property access is not callable. |
| Method call | `log.close()` | Consuming methods invalidate the receiver immediately. |
| Direct resource call | `@("log.txt").close()` | Late resource construction is still statically resolved before lowering. |
| Receiver reference | `@file` inside `@file::name` body | Refers to the receiver instance, not a constructor. |

## Tasks

| Area | Canonical form | Notes |
|------|----------------|-------|
| Single task | `job = ||> { ... }` | Produces a task handle. |
| Task group | `||> [ t1 := { ... } t2 := { ... } ]` | Each entry binds a task handle. |
| Await | `?| job -> value: T | fallback` | Fallback is optional. |
| Resource order | `t1 => t2` | Explicit happens-before edge; it does not transfer data. |

## Rejected Families

The active syntax docs name rejected families only as migration boundaries:

1. Retired M6 state containers and state references are parser errors.
2. Source-level bare `@` is not an authoring form.
3. Reserved wave tokens `<~` and `~>` remain fail-closed.
4. Compatibility spellings that remain implemented must not be used as the default teaching surface.
