# Styio Language Design Specification

**Purpose:** Styio 语言的 **权威语义与特性说明**（正文规格）；形式文法见 [`Styio-EBNF.md`](./Styio-EBNF.md)，符号与 token 名见 [`Styio-Symbol-Reference.md`](./Styio-Symbol-Reference.md)，`@` **目标**拓扑见 [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md)，当前实现缺口见 [`../rollups/NEXT-STAGE-GAP-LEDGER.md`](../rollups/NEXT-STAGE-GAP-LEDGER.md)。

**Last updated:** 2026-05-31

**Version:** 1.0-draft  
**Date:** 2026-03-28  
**Status:** Active Design — Pre-stabilization

---

## 1. Introduction

Styio is an **intent-aware, symbol-driven stream processing language** designed for high-performance resource dispatching, with an initial target domain of **financial quantitative analysis**. It compiles through LLVM to native code, achieving C++-level performance with a fraction of the syntactic overhead.

The name encodes the language's identity:
- **St** — Stream Computing
- **y** — Style / Syntax
- **io** — Native I/O primitives

### 1.1 Design Pillars

| Pillar | Description |
|--------|-------------|
| **Pure Symbolism** | Replace natural-language keywords (`if`, `while`, `for`, `def`) with unambiguous symbolic operators (`?=`, `>>`, `#`, `@`). |
| **Intent Awareness** | The compiler statically analyzes field access patterns and pushes intent down to resource drivers (e.g., only fetch needed database columns). |
| **Honest Missing** | Runtime absence is represented as `@` in diagnostics and stream algebra. Source-level bare `@` is retired from active syntax; current code should obtain absence from resources or intrinsics instead of authoring it directly. |
| **Thick Library, Thin Artifact** | Development uses a rich standard library with protocol detection and AI-assisted probing. Production builds perform dead-code elimination to produce minimal binaries. |

### 1.2 Compiler Toolchain

- **Language:** C++20
- **Backend:** LLVM 18+ (IRBuilder + ORC JIT)
- **Parser Strategy:** Hand-written recursive descent, LL(n) with lookahead
- **Dependencies:** LLVM, ICU，及测试用 GoogleTest、cxxopts 等 — **完整清单与许可** 见 [`../specs/THIRD-PARTY.md`](../specs/THIRD-PARTY.md)。

---

## 2. Core Philosophy

### 2.1 Everything Is a Flow

Styio has no explicit loop constructs (`for`, `while`). Instead, data sources emit **pulses** into **closures** via the pipe operator `>>`. In `left >> right`, the left side is an iterable or pulse source; `>>` advances it one item at a time and pushes each item as a pulse into the right-side channel, closure, resource sink, or pipeline stage. The closure executes once per pulse. Loops emerge naturally from infinite or finite data generators.

### 2.2 Progressive Performance

The language follows a "write less, get convenience; write more, get speed" model:
- Omit type annotations → compiler infers defaults (`i32` for integers, `f64` for floats)
- Add explicit types → compiler generates optimized, specialized instructions
- Omit resource protocol → runtime probes automatically
- Specify protocol (e.g., `@file`, `@mysql`) → static dispatch without runtime probing

### 2.3 Expression-Oriented

All control flow constructs (match, conditional wave, loops) are **expressions** that produce values. There are no void statements — everything flows.

---

## 3. Type System

### 3.1 Primitive Types

| Type | Bits | Description |
|------|------|-------------|
| `bool` | 1 | Boolean |
| `i8`, `i16`, `i32`, `i64`, `i128` | 8–128 | Signed integers |
| `f32`, `f64` | 32, 64 | IEEE 754 floating point |
| `char` | variable | Unicode character |
| `string` / `str` | variable | UTF-8 string |
| `byte` | 8 | Raw byte |

### 3.2 Default Types

When type annotations are omitted:
- Integer literals default to `i32`, including negative literals such as `-1`
- Floating-point literals default to `f64`, including negative literals such as `-1.5`

### 3.3 Type Annotations

Types are annotated with `:` on both parameters and return values:

```
# add : f32 = (a: f32, b: f32) => a + b
```

- `add : f32` — return type is `f32`
- `a: f32` — parameter type
- `:` always binds a **type** to its left-hand identifier
- `m: matrix = [[1,0],[0,1]]` — explicit matrix context accepts a nested list source form, checks that all rows are non-empty, rectangular, and numeric, and lowers the value to a matrix handle; untyped nested lists remain ordinary lists

### 3.4 Matrix Values

`matrix` is a typed numeric collection, not a universal nested-list mode. The parser keeps
`[[...], [...]]` as an ordinary list literal unless the surrounding type context explicitly says
`matrix`; this avoids paying rectangular-shape checks for every nested list expression.

```styio
m: matrix = [[1,0],[0,1]]
```

Matrix binding rules:

- rows must be non-empty and rectangular
- elements must be numeric, with mixed integer/float values promoted to `f64`
- statically known dimensions are preserved in the inferred type
- shape mismatches are semantic errors before lowering
- `m[row][col]` reads one element, while `m[row]` materializes a list row

Matrix operators and functions:

| Surface | Meaning |
|---------|---------|
| `a + b`, `a - b` | element-wise matrix add/subtract; shapes must match |
| `a * b` | matrix multiply when both operands are matrices |
| `a * scalar`, `scalar * a` | scalar multiplication |
| `mat_add`, `mat_sub`, `mat_hadamard`, `matmul` | explicit matrix arithmetic helpers |
| `transpose`, `dot`, `norm`, `mat_sum` | common numeric reductions/transforms |
| `mat_zeros`, `mat_zeros_i64`, `mat_identity`, `mat_identity_i64` | constructors |
| `mat_shape`, `mat_rows`, `mat_cols`, `mat_get`, `mat_set`, `mat_clone` | shape, access, mutation, and copy helpers |

The current runtime representation is a flat row-major matrix handle with element-kind-specific
helpers for `i64` and `f64`. Small statically shaped same-type operations may lower directly to
LLVM loads/stores over the flat backing store; dynamic, mixed-kind, or larger operations route
through the runtime helper surface registered in ORC.

### 3.5 Runtime Absence: `@`

`@` represents **honest absence** at runtime and in diagnostics. It is not `null`, not `0`, not `NaN` — it is the explicit admission that data does not exist.

**2026-04-24 syntax revision:** user-authored bare `@` is no longer part of the
active source language. Historical fixtures such as `x = @`, `x + @`,
`x -> @stdout`, and the old wave-dispatch sink shorthand were retired from active feature fixtures.
`@` remains visible as an absence marker produced by resources/intrinsics and in
diagnostics.

**Propagation rules:**
- absence produced by resource/intrinsic execution propagates through supported
  arithmetic and logical operators
- absence short-circuits through expressions until explicitly intercepted

**Diagnostic tainting (debug mode):**
In debug builds, `@` carries metadata (reason code, source location) enabling root-cause tracing via `.reason()`.

---

## 4. Module Imports

Styio uses explicit top-level imports to declare module dependencies across `.styio` files.

### 4.1 Import Declaration

```text
@import { styio/mod, styio.mod; core }
```

Rules:

- `@import` is only valid at file top level.
- `/` is the native package and module path separator.
- `.` is accepted as a compatibility spelling and is normalized to slash form internally.
- A single import item must not mix `.` and `/`.
- `,` and `;` are equivalent separators between import items.
- Empty import lists, trailing separators, and the legacy leading string-list form such as `["pkg"]` are syntax errors.

### 4.2 Resolution Semantics

Each import item creates one explicit import fact for the current file. The IDE and HIR layers expose these facts in canonical slash form, so `styio.mod` and `styio/mod` both resolve as `styio/mod` internally.

Import resolution remains explicit:

- bare package paths are resolved through the project-aware import lookup rules
- `.styio` is tried when the import candidate does not already name a Styio file
- unresolved imports stay unresolved instead of binding to unrelated same-text symbols elsewhere in the workspace

---

## 5. Functions

### 5.1 Definition Syntax

Functions are declared with `#`:

```
# add := (a, b) => { <| a + b }    // block form with explicit yield
# add := (a, b) => a + b            // expression form (implicit yield)
# add := (a: i32, b: i32) => a + b  // with type annotations
# add : i32 = (a: i32, b: i32) => a + b  // with return type
```

### 5.2 Anonymous Closures

Used within stream pipes:

```
prices >> #(p) => { <| p * 2 }
```

`#(p)` binds the current pulse to the local name `p`.

### 5.3 Context Capture with `$(...)`

Functions can explicitly capture external variables by reference:

```
trade $(bal, is_open) := my_strategy <| bal <| is_open
```

The `$(...)` list declares a **reactive binding** — the function re-evaluates whenever captured variables change.

---

## 6. Control Flow

Styio's control flow is entirely **symbol-driven** and **expression-oriented**.

### 6.1 Pattern Matching: `?=`

```
x ?= {
    1  => { <| "one" }
    2  => { <| "two" }
    _  => { <| "other" }
}
```

- `?=` — match operator (condition probe)
- `=>` — pattern-to-result mapping
- `_` or an all-underscore identifier such as `_______` — wildcard / default branch
- `<|` — yield (explicit return from block)

The binding form matches a scrutinee while exposing it to every arm:

```
#(n = values.length) ?= {
    0 => { /* empty */ }
    1 => { answer = values[0] }
    _ => { /* n is available here */ }
}
```

For integer match lowering, literal arms (`1 => ...`) and guarded equality arms
that compare the scrutinee to an integer (`(n == 1) => ...`) are semantically
the same arm. AST lowering emits ordinary StyioIR, then the StyioIR optimizer
canonicalizes equivalent match shapes before LLVM codegen so accepted source
spellings can produce identical switch-shaped LLVM IR.

Current executable match expression and function match sugar result families are
`i64`, `f64`, `bool`, `char`, and `string`; tuple/container result families fail
closed until their value IR and merge semantics are defined.

### 6.2 Infinite Loop

```
[...] => { /* body */ }
```

`[...]` is an infinite pulse generator. The closure executes indefinitely.

### 6.3 Conditional Loop (While-equivalent)

```
[...] >> ?(expr) => { /* body */ }
```

- `[...]` — infinite generator
- `>>` — pulse the generator one item at a time into the workflow
- `?(expr) =>` — guard / valve: only passes pulses into the body when `expr` is truthy

### 6.4 Collection Iteration (For-each-equivalent)

```
[1, 2, 3] >> #(item) => { /* body */ }
```

The collection becomes a finite pulse source. `>>` advances the collection one element at a time and pushes each element as a pulse into the closure; each pulse binds one element to `item`.

### 6.5 Break: `^...` (Immediate Loop)

```
^       // break out of the nearest enclosing loop
^^      // same as ^
^^^^    // same as ^
```

Rules:
- `^` characters must be **contiguous** (no spaces)
- any contiguous run of `^` is one break statement
- the count of `^` characters has no semantic depth and is normalized to 1
- `^^ ^^` is **illegal** — it is two adjacent break statements, not a deeper break
- a break outside an enclosing loop is rejected by code generation

### 6.6 Continue: `>>...` (Standalone, Variable Length, >=2)

```
>>       // skip the rest of the current block for this pulse/session
>>>      // same as >>
>>>>     // same as >>
>>>>>>   // same as >>
```

The base continue spelling is 2 characters (`>>`), and any longer contiguous run of `>` characters has the same meaning when it is a standalone statement. The count of `>` characters has no semantic depth and is normalized to one continue operation.

Context distinguishes continue from pipe: the pipe form connects a left iterable or pulse source to a right channel/consumer (`left >> right`). The continue form is a standalone statement: aside from horizontal whitespace, the `>>...` token is the whole line/statement and is followed by a newline, statement separator, block end, or EOF. In a pulse/session domain, it skips the remaining statements in the current block and resumes at the next pulse/session of the nearest continue-capable domain. Outside such a domain, current code generation rejects it as `continue outside enclosing loop`.

### 6.7 Yield / Return: `<|`

```
# square := (x) => { <| x * x }
```

`<|` pushes a value out of the current block. When used in control flow that is part of an assignment, it produces the value for the enclosing expression.

In expression position, `<|` applies one value to a callable/continuation. Chained
apply-pipe examples are not canonical while continuation lowering remains pending.

Captured continuations follow the OCaml-style one-shot discipline: a suspended continuation must be resumed or discontinued exactly once. Resuming it consumes it; resuming it again is an error. While suspended, it keeps captured scope data and resources alive until resume/discontinue unwinds the frame.

For compressed one-line blocks, `|<| value |;` is the inline return spelling. `|>` and `|<-` remain reserved.

When multiple branches yield (e.g., in `?=`), the compiler generates LLVM `phi` nodes at the merge point.

### 6.8 Block-Entry Snapshot / Commit

Every language form that enters a `{ ... }` block creates a resource snapshot context at the
block-entry operator and commits resource effects at the matching `}`. This is a general block
semantics rule, not a special case for resource loops.

Covered block-entry surfaces include `>>`, `=>`, `?=`, active `||>`, and the reserved `|>` family
if it later becomes an active block-entry form. The rule applies to resource state and resource
effects; ordinary lexical values keep their normal scoping rules.

Chained stages commit once per block. For example:

```styio
a => { 1 } => { 2 } => { 3 }
```

creates three snapshot/commit units. The second block starts from the state committed by the first
block, and the third starts from the state committed by the second.

If a block-entry operation supports recovery, recovery must wrap the
block-entry operation with `?| block_entry_operation | fallback`. A trailing
`} | fallback` is not resource fallback.

### 6.9 Tasks, Resource Effects, and Await: `||>` / `?|`

`||> { ... }` constructs one scheduled task. `||> [ name := { ... } ... ]`
launches a group of independent task blocks and binds each name to its task handle.

```styio
||> [
    price := { <| fetch_price() }
    risk  := { <| calc_risk() }
]

?| price -> p: f64
?| risk -> r: f64 | 0.0
?| @("log.txt").close()
?| @("archive.log").close() | cleanup_failure => report_cleanup()
```

`?| task -> value: T` awaits or pulls a task/future handle and declares `value`
with type `T`. Without fallback, a failed pull settles at that source site and
raises a structured error immediately. `?| task -> value: T | fallback` evaluates
`fallback` only when the task pull reports runtime failure or absence.

The same marker is the uniform resource-effect evaluation form:
`?| resource_operation` settles the resource operation in place, while
`?| resource_operation | fallback` recovers through normal type inference. The
successful operation value and fallback value must match the surrounding use-site
type. Current statement forms include file writes, file acquire/rebind,
direct file line iteration such as
`?| @file("data.txt") >> #(line) => { ... } | fallback`, and resource method
calls; file iterator open failures recover through catch-all fallback or matched
`io` handlers, while non-file iterators remain rejected under `?|`. Current
value-producing forms include file/stdin instant pulls,
acquired file-handle instant pulls after a checked file acquire, materialized
container bounds reads, including ordered dict value slices that lower through
`d.values` plus list-slice bounds recovery, and user-defined resource methods
whose body is a single `<| expr` return, a statement-only preface followed by a
final `<| expr` return, or scalar local `=` / `:=` value-binding prefaces or local list/dict/matrix `=` / `:=`
prefaces followed by a final scalar/string or local list/dict/matrix container `<| expr` return. Method-local
scalar/list/dict/matrix `=` and `:=` bindings are scoped to the inlined method body;
returning a local list/dict/matrix container clones the handle before method-scope cleanup, while local resource bindings and
capture-dependent method bodies remain rejected until those scope semantics are implemented. A
bare `resource_operation | fallback` is not a resource fallback form.
Effect-specific handlers use the same boundary:
`?| resource_operation | effect_name => handler` handles only the named typed
effect family. For example, `?| res -> msg_queue | backpressure => do_something()`
is a pressure handler, not a `?=` match and not a catch-all fallback.
Handlers may be chained: `?| op | e1 => handler1 | e2 => handler2`.
The current statement implementation covers file acquire/rebind, direct file
iterators, direct file release to `@()`, file resource-method settlement, and
file writes under this boundary, while value-producing recovery remains limited
to the explicitly listed instant-pull, container-bounds, and value-returning
resource-method slices.
`?| resource_operation | ...` is an audited discard form, but only as a
standalone statement. It executes and settles the resource operation, discards
business recovery for any effects at that site, produces no value, and continues
with the next statement. It is illegal anywhere a value is required, such as
assignment, argument position, or branch expression. `?| resource_operation |
effect => @()` is rejected: `@()` is an empty resource / destroy sink, not an
executable empty action.
`?=` remains value/pattern matching; it can match `backpressure` only after an
operation explicitly materializes a normal result value, not as an implicit catch
for unsettled resource effects.
`??` remains diagnostic extraction, not async or resource fallback syntax.

Backpressure is a resource pressure signal before it is a failure. A pressured
write may wait, stay pending, or be scheduled by the resource family. Only an
escalated pressure condition, such as a closed channel, failed transport,
timeout, or exceeded backlog limit, becomes a `ResourceBackpressureFailure` for
`?| ... | fallback`.

Resource families may expose pressure as an ordinary observable effect stream:

```styio
?| res -> msg_queue | backpressure => do_something()
?| res -> msg_queue | ...

channel.pressure >> #(p) => {
    ?(p.pending > 10000) => {
        ||> {
            ?| channel.inspect()
              | inspection_failed => report_pressure_probe(p)
        }
    }
}
```

This form is side-effecting resource code, not implicit error handling. The
observer can count, log, spawn a task, or invoke recovery operations, and those
operations still obey normal resource capabilities and fallback rules.
The current compiler recognizes the observer syntax and rejects every current
resource family in Sema with `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`
until a family declares a pressure payload and runtime stream.

This pressure model is a core Styio design choice. The compiler may do additional
effect inference when that preserves a valuable language feature: useful resource
effects should not be collapsed into failures merely because they require more
static reasoning. Backpressure is useful precisely because it can remain
observable and mostly harmless until a resource-family policy escalates it.

`?| -> value: T` is reserved as the bare "freeze here" continuation point. The
parser accepts the shape, but semantic analysis currently fails closed until
first-class continuation lowering can guarantee one-shot resume/discontinue.

---

## 7. Guard Conditionals

Guard conditionals replace ternary expressions and if/else chains with a single
condition-first spelling. The old wave spellings are tokenized but reserved:
`<~` and `~>` have no active user-level semantics.

### 7.1 Inline Guard Value: `?(cond) => A | B`

```
val = ?(a > b) => a | b
```

Read as: "If condition holds, evaluate to `a`; otherwise evaluate to `b`." This is the canonical inline value-selection form.

### 7.2 Block Guard: `?(cond) =>`

```
?(signal) => {
    order_logic(p)
} | {
    fallback_logic(p)
}
```

Read as: "If signal is truthy, execute the block; otherwise execute the fallback block." When the fallback block is omitted, the false branch routes to `@` (void).

### 7.3 Visual Semantics

| Form | Meaning |
|------|---------|
| `?(cond) => A \| B` | Inline value selection |
| `?(cond) => { A } \| { B }` | Block-level if/else |
| `\|` | Else/fallback separator |

---

## 8. Resource System

### 8.1 Resource Identifiers: `@`

Resources are accessed via the `@` prefix:

```
@("localhost:8080")          // auto-detect protocol
@file("readme.txt")          // explicit file protocol
@mysql("localhost:3306")     // explicit MySQL protocol
@binance("BTCUSDT")         // exchange data feed
```

**Protocol resolution:**
- `@{...}` or `@(...)` without prefix → runtime probes via plugin dictionary
- `@protocol(...)` with prefix → compile-time static dispatch (zero overhead)

### 8.2 Handle Acquisition: `<-`

```
f <- @file("readme.txt")
```

`<-` extracts a live handle (file descriptor, socket, cursor) from a resource.

### 8.3 Reading: `>>`

```
f >> #(chunk: [byte; 4096]) => { buf += chunk }
```

For resource reads, `>>` treats the left resource handle as an iterable source and pushes each produced chunk or line as a pulse into the right-side closure.

### 8.4 Writing: `<<`

```
"Hello Styio" << f
```

### 8.5 Lifecycle: Scope-based RAII

Resources are automatically released when their enclosing scope ends. The
compiler inserts cleanup code at every exit path, including normal scope exit,
loop `^` / `>>` control-flow exits, and `<|` returns. Current implementation
evidence covers tracked file handles for those paths. It also covers the
default file flex-rebind cleanup boundary: before `name = @file(...)`
overwrites an existing tracked file handle, codegen closes the old handle,
checks the cleanup error channel, and stops before the new acquire or later
statements if cleanup failed. Statement-shaped file rebind can now install an
explicit settlement site with `?| name = @file(...) | fallback` or named
handlers; under that wrapper, the old-handle cleanup error remains on the
resource-effect channel instead of continuing into the replacement open.
Broader resource-family cleanup and source-level fallback recovery for implicit
cleanup remain staged implementation work.

### 8.6 Persistence via Redirection: `->`

```
ma5 -> @database("redis://localhost/ma5_cache")
```

`->` redirects a value's storage destination. The runtime asynchronously syncs to the target resource.

### 8.7 Standard Stream Resources

Styio models the three Unix standard streams as **compiler-recognized resource atoms** over a
single built-in terminal handle, canonically written `[>_]`. The parenthesized terminal device
`(>_)` remains a compatibility spelling for parser/runtime surfaces that already use it.

The current frozen grammar accepts:

```
@stdout
@stderr
@stdin
```

directly in source code. Users do not need to repeat the internal prelude declarations before
using these standard streams. The declarations still exist as Styio source in the resource prelude
rather than as a C++ resource-name registry.

**`>_` — The Terminal Device**

`>_` is the first-class terminal device value. In symbolic standard-stream definitions, the
bracketed terminal-handle spelling `[>_]` is canonical:

| Operation | Canonical symbolic form | Compatibility form | Unix fd | Semantics |
|-----------|--------------------------|--------------------|---------|-----------|
| Scalar write | `x -> [>_]` | `x -> (>_)` | fd 1 | Write one scalar/text value to stdout |
| Iterable write | `xs >> [>_]` | `xs >> (>_)` | fd 1 | Advance `xs` item by item; write each item as a pulse to stdout |
| Scalar error write | `!(x) -> [>_]` | `!(x) -> (>_)` | fd 2 | Write one scalar/text value to stderr (unbuffered) |
| Iterable error write | `!(xs) >> [>_]` | `!(xs) >> (>_)` | fd 2 | Advance `xs` item by item; write each item as an unbuffered pulse to stderr |
| Read stream shorthand | `<\|[>_]` | `<\|(>_)` | fd 0 | Return the terminal input stream |
| Read stream expanded | `<\| <- [>_]` | `<\| <- (>_)` | fd 0 | Pull the terminal input stream, then return it |

`!()` acts as a **channel selector**: without `!`, data goes to fd 1 (stdout); with `!`,
data goes to fd 2 (stderr). The compiler disambiguates from logical NOT by context:
`!(expr) -> ( >_ )` is always channel-select.

`expr -> [>_]` and `expr -> @stdout` are scalar/text redirects to stdout. `items >> [>_]`
and `items >> @stdout` are narrower: the left side
must be an iterable value whose items can be serialized to text, such as `list[T]`, `dict[string,T]`,
or an explicitly produced line list. `>>` serializes and writes each item as a separate pulse into the stream sink. Plain `string >> [>_]` and `string >> @stdout` are rejected so the compiler never has
to guess between character iteration and newline splitting. Use `string -> [>_]` for scalar text,
or `string.lines() >> [>_]` / `string.lines() >> @stdout` when newline splitting is intended.

**@stdout** — write-only, system-default buffering (line-buffered for TTY, block-buffered for pipes).

**@stderr** — write-only, **unbuffered** (immediate `fflush(stderr)` after each write).

**@stdin** — read-only, iterable stream. The canonical internal declarations are:

```styio
@ stdin := #() => { <|[>_] }
@ stdin := #() => { <|(>_) }
@ stdin := #() => { <| <- [>_] }
@ stdin := #() => { <| <- (>_) }  // compatibility terminal-device spelling
```

`<|(>_)` is a call-like shorthand: `<|` supplies the exported value and `(>_)` is the
terminal-device argument. `[>_]` replaces the earlier `| >_ |` spelling to avoid a `|>`
visual/tokenization ambiguity. `@stdin >> #(line) => {...}` iterates lines.
EOF terminates iteration naturally. New design text should not use `<<` for stdin reads or
`lines << @stdin` for implicit collection; collect explicitly inside the iterator body or through
a named typed-read API. Older frozen docs and implementations accepted `(<< @stdin)` as instant
pull; treat that as a compatibility artifact, not the canonical read/pull spelling.

**Write syntax:**

```
42 -> @stdout              // redirect value to stdout (action)
"Hello" -> @stdout         // redirect string to stdout
@stdout("Hello")           // call form (freezes for continuation)
```

Feature test catalog use `-> @stdout` / `-> @stderr` as the **canonical spelling**.
The current compiler also accepts iterable sink writes:

```
values >> @stdout
text.lines() >> @stdout
warnings >> @stderr
records >> @file("out.txt")
```

When `>>` is followed by a writable resource atom (`@stdout`, `@stderr`, or `@file(...)`), the
parser builds a `resource_write` node. The semantic rule matches terminal-handle `>> [>_]`:
the left side must be iterable and text-serializable. Use `->` for scalar values and `>>` only
where sink-pulse style is intentional.

**Direction constraints:**

- `@stdin` is read-only: `expr -> @stdin` and `expr >> @stdin` are semantic errors
- `@stdout` / `@stderr` are write-only: `@stdout >> #(x) => {...}` is a semantic error
- Standard streams need no repeated user-authored declarations; `f <- @stdout` is a semantic error

**Compiler recognition:** The compiler recognizes `@stdout`, `@stderr`, `@stdin` directly at
parse/lowering time and emits FFI-backed standard-stream runtime-helper IR (`styio_stdout_write_cstr`
for stdout writes, `styio_stderr_write_cstr` for stderr writes, and `styio_stdin_read_line` for
stdin line reads). Scalar `expr -> @stdout` lowers directly through the standard-stream write
IR family. Iterable `items >> @stdout` and `items >> @file(...)` first expand into a per-item
pulse loop, and the loop body writes each item through the matching sink-write IR family. The
`>>` route requires text-serializable iterable input before lowering.

---

## 9. State Management

### 9.1 The Problem

Stream processing requires **memory across pulses**. A simple local variable resets every frame. Styio solves this with explicit state containers.

### 9.2 Resource Object State

resource topology treats named state as a resource object:

```styio
@price : f64|..10| := { ... }
```

Bare `@price` is the resource object itself. It is not the latest scalar value.

```styio
latest = @price[-1]
prev   = @price[-2]
recent = @price[-3..]
all    = @price[...]
```

`T|n|` means exact length; `T|..n|` means a recent-window resource that keeps the latest `n` values. Unbounded repetition is written with a type suffix:

```styio
i64|10|     // ten i64 values
f64|..10|   // latest ten f64 values
i64..       // unbounded i64 sequence
i64...      // same as i64..
```

### 9.3 Type-Level Collection Sugar

Collection types are ordinary type-position forms:

```styio
__ : list[T] := T..
__ : string := char..
__ : dict[K, V] := (K, V)..
```

Examples:

```styio
@input : i64|10| := ...
@meta  : dict[string, string] := ...
@pairs : (string, string)|2| := ...
@price : f64|..10| := ...
@log   : string := ...
@logs  : list[string] := ...
```

`list[i64]|10|` is not the canonical spelling for ten integers; it normalizes to `i64|10|`. Prefer the direct length form.

### 9.4 Resource Flow and Copy

```styio
expr -> @price
@price >> #(x) => { ... }
snapshot << @price[...]
```

`->` writes a produced value into a resource sink. `>>` enters a snapshot-backed block or iteration and commits resource effects at block exit. `<<` makes an explicit copy or snapshot.

Resource acquisition uses `<-` only at resource-entry boundaries:

```styio
l <- @stdin: list[i32]
l1 << l
```

Copying an already-bound resource as `l1 <- l` is rejected; use `l1 << l`.

### 9.5 Retired state-resource State Families

The old state-resource state-declaration and shadow-read families are retired parser errors. Active code must
use resource objects:

```styio
@ma5 : f64|..5|
get_ma(prices, 5) -> @ma5

@total_vol : f64|..1|
next_total = @total_vol[-1] + volumes
next_total -> @total_vol
```

The compiler rejects retired state-family spellings with migration diagnostics. Exact old wording
is recoverable from Git history when needed.

### 9.6 Retired History Probe Family

The old state-resource history-probe selector family is not active syntax.

---

## 10. Stream Synchronization

### 10.1 Aligned Sync (Zip): `&`

```
@binance >> #(p) & @okx >> #(p_okx) => {
    arbitrage_gap = p - p_okx
}
```

Both streams must deliver a pulse before the closure executes. The trigger frequency is `min(freq_A, freq_B)`.
Finite zip also applies to the standard input line stream when the other side is a materialized
list or an `@file` stream:

```styio
@stdin >> #(label) & prices >> #(price) => {
    >_(label + " " + price)
}
```

This consumes at most one stdin line per matched frame and terminates at stdin EOF or the
shorter finite peer. Zipping `@stdin` with itself is not active syntax until duplicate
consumption of the same external stream has a driver-level decision.

Optional tolerance window:

```
@binance >> #(p) &[5ms] @okx >> #(p_okx) => { ... }
```

### 10.2 Snapshot Pull: `<< @resource`

```
@binance >> #(p) => {
    p_okx << @okx("BTC")
    gap = p - p_okx[-1]
}
```

The explicit `<<` copy makes the snapshot boundary visible. New topology text should prefer
resource-object or snapshot-object reads such as `p_okx[-1]`.

Inline immediate pull (no state declaration, live read):

```
gap = p - (<- @okx("BTC"))
```

### 10.3 Synchronization Summary

| Mode | Syntax | Trigger | Use Case |
|------|--------|---------|----------|
| Zip | `A >> #(a) & B >> #(b)` | Both arrive | Atomic cross-exchange arbitrage |
| Snapshot | `v << @res` + `v[-1]` | Main flow only | Cross-frequency reference |
| Immediate Pull | `(<- @res)` | On-demand | One-shot live sampling |

---

## 11. Selector Operators: `[mode, arg]`

Square brackets serve as a **contextual transformer**, not just an indexer.

### 11.1 Static Indexing and Slicing

```
a[0]        // element at index 0
a[-1]       // last element / latest committed value
a[0..5]     // slice from 0 to 5
a[2..]      // slice from index 2 to end
a[..5]      // slice from start to index 5
a[..]       // all values
a[...]      // all values
```

Two or more dots are equivalent in selectors: `a[0..5]`, `a[0...5]`,
and `a[0.....5]` use the same range separator. A single dot remains
member access, as in `a.length`.

### 11.2 Retired Guard Selector: `[?, cond]`

The postfix guard selector was an early draft and is no longer active syntax.
Use `?(cond) => value | fallback` for value selection, or normal `?(cond) => { ... }`
blocks for statement-level control.

### 11.3 Retired Equality Probe: `[?=, val]`

The postfix equality probe was retired with the guard selector. Use `?=` match
blocks for equality-style branching.

### 11.4 Plugin Operators: `[op, n]`

```
prices[avg, 20]    // moving average compiler intrinsic in the pulse/state path
prices[max, 14]    // rolling maximum compiler intrinsic in the pulse/state path
```

Current compiler-owned series intrinsics are limited to `avg` and `max`, with the
implementation and limits recorded in
[Styio-StdLib-Intrinsics.md](./Styio-StdLib-Intrinsics.md). Other proposed
operators such as `min`, `std`, `ema`, and `rsi` are deferred until they have
parser, Sema, lowering, runtime/codegen, and test evidence.

### 11.5 Retired History Probe Family

The old state-resource postfix history selector family is not active syntax. Future history access
must use resource selectors or a revised state-topology fixture.

Historical examples remain provenance only in Git history.

---

## 12. I/O and Side Effects

### 12.1 Terminal Device: `>_`

`>_` is the **terminal device primitive** — a first-class resource handle representing the
user's terminal. Symbolic standard-stream definitions write it canonically as `[>_]`, with
`(>_)` retained as a compatibility spelling. All standard streams (`@stdout`, `@stderr`,
`@stdin`) are compiler-recognized resource atoms over this terminal device. See §8.7 for the
complete resource definitions and usage patterns.

**As a print statement (legacy, backward-compatible):**

```
>_("Hello, Styio!")
>_(variable)
```

`>_(expr)` remains functional for backward compatibility and is internally equivalent to
`expr -> ( >_ )`.

**As a value in expressions:**

```
42 -> @stdout                        // >_ used as redirect target under the hood
@stdin >> #(line) => { >_(line) }     // >_ used as stream source under the hood
```

**Type formatting rules** (applies to `>_()`, scalar `-> @stdout`, and each item emitted by iterable `>> @stdout` / `>> @file(...)`):

| Type | Output format |
|------|---------------|
| Integer | `%lld\n` |
| Float | `%.6f\n` |
| Bool | `true\n` / `false\n` |
| String | `%s\n` |
| Char | `%c\n` |
| `@` (Undefined) | `@\n` |

### 12.2 I/O Buffer: `>_` (stream context)

In stream contexts, `>_` writes to the system's buffered output channel.

### 12.3 Format Strings: `$`

```
$"Price is {p}, Volume is {v}" -> @stdout
```

### 12.4 Standard Error: `@stderr`

`@stderr` writes to Unix fd 2 with immediate flush. See §7.7 for definition.

```
"Error: file not found" -> @stderr
```

### 12.5 Standard Input: `@stdin`

`@stdin` is a line stream from Unix fd 0. See §7.7 for definition.

```
@stdin >> #(line) => {
  line -> @stdout
}
```

---

## 13. Error Handling Philosophy

### 13.1 Fail-Fast for Structural Errors

If a resource schema mismatch is detected (e.g., accessing a non-existent database column), the program terminates immediately at connection time — **before** the first data pulse. No silent degradation.

### 13.2 Algebraic Propagation for Data Errors

Missing data within a stream becomes runtime absence, displayed as `@` in diagnostics and terminal formatting. It propagates through supported downstream computations; user code should not manufacture this state with a standalone `@` literal.

### 13.3 Diagnostic Tracing

Design target: in debug mode, `@` values may carry tainted metadata:

```
last_signal ?? reason    // "DataSource(@binance) timeout at 14:22:05.123"
```

The `??` operator is deferred until the absence-metadata contract, parser surface,
type behavior, and runtime route are implemented.

### 13.4 Guard-based Recovery

```
safe_price = price | @last_valid_price[-1]    // value fallback if price carries runtime absence
```

Value-level absence fallback is a design target. Current implemented recovery
evidence is the resource-effect form `?| operation | fallback` and its named
handler variants described in the active test catalog.

The `|` operator provides a value fallback when the left side carries runtime
absence. Resource failures do not use bare `|`; they use
`?| resource_operation | fallback`.

---

## 14. Compilation Modes

### 14.1 Development Mode (JIT)

- Full standard library loaded
- AI-assisted protocol probing enabled
- LLVM ORC JIT for instant execution
- Diagnostic tainting active

### 14.2 Strict Mode (AOT, `styio build --strict`)

- All types must be explicitly annotated
- All resource protocols must be specified
- Intent-aware dead code elimination
- Output: minimal native binary or WebAssembly module

### 14.3 Audit Mode (`styio audit --fix`)

- Scans retired state syntax as migration diagnostics and target `@name : Type` resources
- Generates explicit `schema` block at file header
- Reformats source according to Styio style guide

---

## Appendix A: Consultant's Additional Thoughts

### A.1 Reconciling Design with Existing Codebase

The current C++ compiler implementation already has a rich token system, parser, AST, IR, and LLVM codegen. However, significant features from the Gemini design discussion are not yet implemented:

- **Reserved wave tokens** (`<~`, `~>`) already exist at the lexer level but have no active grammar production
- **Target resource objects** (`@name : Type`, `@name[-1]`, `@name[...]`) require a new state/resource analysis pass
- **Pulse Frame Lock** needs runtime infrastructure in the JIT executor
- **Cross-stream sync** (`&`, `<< @res`) requires a concurrency model in the IR

**Recommended implementation order:**
1. Keep `?(cond) => value | fallback` and `?(cond) => { ... } | { ... }` as the active guard forms
2. Extend the parser/type system for `Type|n|`, `Type|..n|`, `Type..`, and `list[T]`
3. Add AST nodes for target resource declarations, resource selectors, and stream zip
4. Implement resource hoisting in the analyzer
5. Extend LLVM codegen for fixed-length and recent-window resource storage
6. Add concurrency primitives for stream synchronization

### A.2 Open Design Questions

1. **`>>` ambiguity resolution:** The parser must distinguish between pipe (`source >> consumer`), continue (`>>` as standalone statement), and stride selector (`[>>, 2]`). The current implementation already handles `>>` as `Iterate` — extending this to multi-meaning requires careful lookahead logic.

2. **`@` overload risk:** `@` remains overloaded as a resource prefix, state prefix, standard-stream prefix, and runtime absence marker. Source-level bare `@` has been retired from active syntax to reduce ambiguity.

3. **Legacy migration:** retired state-resource state families are parser errors. The active surface is `@name : Type|n|` / `@name : Type|..n|` plus resource-object selectors.

4. **Cross-platform builds:** The current CMakeLists.txt hardcodes Linux paths. Windows and macOS support need platform-conditional toolchain detection.

### A.3 Performance Considerations

The **Pulse Frame Lock** design is elegant but may introduce measurable overhead in ultra-high-frequency scenarios (>100k ticks/sec). Consider:
- A compile-time optimization that detects when frame lock is unnecessary (single resource-snapshot read, no aliasing)
- An `unsafe` annotation to opt out of frame lock for latency-critical inner loops
- Hardware-level atomic snapshot using `LOCK CMPXCHG` or ARM `LDXR/STXR` for multi-threaded shadow updates
