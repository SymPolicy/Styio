# Styio Language Design Specification

**Purpose:** Styio 语言的 **权威语义与特性说明**（正文规格）；形式文法见 [`Styio-EBNF.md`](./Styio-EBNF.md)，符号与 token 名见 [`Styio-Symbol-Reference.md`](./Styio-Symbol-Reference.md)，`@` **目标**拓扑见 [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md)，冲突与未定见 [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md)。

**Last updated:** 2026-05-03

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

Styio has no explicit loop constructs (`for`, `while`). Instead, data sources emit **pulses** into **closures** via the pipe operator `>>`. The closure executes once per pulse. Loops emerge naturally from infinite or finite data generators.

### 2.2 Progressive Performance

The language follows a "write less, get convenience; write more, get speed" model:
- Omit type annotations → compiler infers defaults (`i32` for integers, `f64` for floats)
- Add explicit types → compiler generates optimized, specialized instructions
- Omit resource protocol → runtime probes automatically
- Specify protocol (e.g., `@file`, `@mysql`) → zero-overhead static dispatch

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
`x -> @stdout`, and the old wave-dispatch sink shorthand were retired from active milestones.
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
- `_` — wildcard / default branch
- `<|` — yield (explicit return from block)

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
- `>>` — pipe the generator into the workflow
- `?(expr) =>` — guard / valve: only passes pulses into the body when `expr` is truthy

### 6.4 Collection Iteration (For-each-equivalent)

```
[1, 2, 3] >> #(item) => { /* body */ }
```

The collection becomes a finite pulse source. Each element is bound to `item`.

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

### 6.6 Continue: `>>` (Variable Length, ≥2)

```
>>      // skip current iteration (1 level)
>>>     // skip 2 levels
>>>>    // skip 3 levels
```

The base continue is 2 characters (`>>`). Each additional `>` skips one more nesting level. Context distinguishes continue from pipe: continue appears as a **standalone statement** (not connecting source to consumer).

### 6.7 Yield / Return: `<|`

```
# square := (x) => { <| x * x }
```

`<|` pushes a value out of the current block. When used in control flow that is part of an assignment, it produces the value for the enclosing expression.

In expression position, `<|` applies a value to a callable/continuation from left to right:

```
make_discount <| 100 <| 150 == make_discount(100)(150)
```

Captured continuations follow the OCaml-style one-shot discipline: a suspended continuation must be resumed or discontinued exactly once. Resuming it consumes it; resuming it again is an error. While suspended, it keeps captured scope data and resources alive until resume/discontinue unwinds the frame.

For compressed one-line blocks, `|<| value |;` is the inline return spelling. `|>` and `|<-` remain reserved.

When multiple branches yield (e.g., in `?=`), the compiler generates LLVM `phi` nodes at the merge point.

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
@file{"readme.txt"}          // explicit file protocol
@mysql{"localhost:3306"}     // explicit MySQL protocol
@binance{"BTCUSDT"}         // exchange data feed
```

**Protocol resolution:**
- `@{...}` or `@(...)` without prefix → runtime probes via plugin dictionary
- `@protocol{...}` with prefix → compile-time static dispatch (zero overhead)

### 8.2 Handle Acquisition: `<-`

```
f <- @file{"readme.txt"}
```

`<-` extracts a live handle (file descriptor, socket, cursor) from a resource.

### 8.3 Reading: `>>`

```
f >> #(chunk: [byte; 4096]) => { buf += chunk }
```

### 8.4 Writing: `<<`

```
"Hello Styio" << f
```

### 8.5 Lifecycle: Scope-based RAII

Resources are automatically released when their enclosing scope ends. The compiler inserts cleanup code at every exit path (including `^^` breaks and `<|` returns).

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

directly in source code. Unlike earlier planning drafts, users do **not** need to write
wrapper definitions such as `@stdout := ...` before using these standard streams.

**`>_` — The Terminal Device**

`>_` is the first-class terminal device value. In symbolic standard-stream definitions, the
bracketed terminal-handle spelling `[>_]` is canonical:

| Operation | Canonical symbolic form | Compatibility form | Unix fd | Semantics |
|-----------|--------------------------|--------------------|---------|-----------|
| Scalar write | `x -> [>_]` | `x -> (>_)` | fd 1 | Write one scalar/text value to stdout |
| Iterable write | `xs >> [>_]` | `xs >> (>_)` | fd 1 | Serialize an iterable value to stdout |
| Scalar error write | `!(x) -> [>_]` | `!(x) -> (>_)` | fd 2 | Write one scalar/text value to stderr (unbuffered) |
| Iterable error write | `!(xs) >> [>_]` | `!(xs) >> (>_)` | fd 2 | Serialize an iterable value to stderr (unbuffered) |
| Read stream shorthand | `<\|[>_]` | `<\|(>_)` | fd 0 | Return the terminal input stream |
| Read stream expanded | `<\| <- [>_]` | `<\| <- (>_)` | fd 0 | Pull the terminal input stream, then return it |

`!()` acts as a **channel selector**: without `!`, data goes to fd 1 (stdout); with `!`,
data goes to fd 2 (stderr). The compiler disambiguates from logical NOT by context:
`!(expr) -> ( >_ )` is always channel-select.

`expr -> [>_]` and `expr -> @stdout` are scalar/text redirects to stdout. `items >> [>_]`
and `items >> @stdout` are narrower: the left side
must be an iterable value whose items can be serialized to text, such as `list[T]`, `dict[string,T]`,
or an explicitly produced line list. Plain `string >> [>_]` and `string >> @stdout` are rejected so the compiler never has
to guess between character iteration and newline splitting. Use `string -> [>_]` for scalar text,
or `string.lines() >> [>_]` / `string.lines() >> @stdout` when newline splitting is intended.

**@stdout** — write-only, system-default buffering (line-buffered for TTY, block-buffered for pipes).

**@stderr** — write-only, **unbuffered** (immediate `fflush(stderr)` after each write).

**@stdin** — read-only, iterable stream. The canonical symbolic definitions are:

```styio
@stdin := { <|[>_] }
@stdin := { <|(>_) }
@stdin := { <| <- [>_] }
@stdin := { <| <- (>_) }  // compatibility terminal-device spelling
```

`<|(>_)` is a call-like shorthand for the same symbolic definition: `<|` supplies the exported
value and `(>_)` is the terminal-device argument. `[>_]` replaces the earlier `| >_ |` spelling
to avoid a `|>` visual/tokenization ambiguity. `@stdin >> #(line) => {...}` iterates lines.
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

Frozen milestone docs use `-> @stdout` / `-> @stderr` as the **canonical spelling**.
The current compiler also accepts iterable stream-sink writes:

```
values >> @stdout
text.lines() >> @stdout
warnings >> @stderr
```

When `>>` is followed by a standard-stream resource atom (`@stdout` / `@stderr`), the parser
builds a `resource_write` node. The semantic rule matches terminal-handle `>> [>_]`: the left
side must be iterable and text-serializable. Use `->` for scalar values and `>>` only where
stream-sink style is intentional.

**Direction constraints:**

- `@stdin` is read-only: `expr -> @stdin` and `expr >> @stdin` are semantic errors
- `@stdout` / `@stderr` are write-only: `@stdout >> #(x) => {...}` is a semantic error
- Standard streams need no user-authored wrapper declarations; `f <- @stdout` is a semantic error

**Compiler recognition:** The compiler recognizes `@stdout`, `@stderr`, `@stdin` directly at
parse/lowering time and emits direct FFI-backed standard-stream IR (`printf`/`puts` for
stdout, `fprintf(stderr, ...)` for stderr, `fgets(stdin)` for stdin). Scalar `expr -> @stdout`
and iterable `items >> @stdout` both lower through the standard-stream write IR family, with
the `>>` route requiring text-serializable iterable input before lowering.

---

## 9. State Management

### 9.1 The Problem

Stream processing requires **memory across pulses**. A simple local variable resets every frame. Styio solves this with explicit state containers.

### 9.2 State Container: `@[...]`

**Superseding narrative (target syntax, not yet default in the compiler):** See [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md) for `@name : [|n|]`, top-level `:= { driver }`, and **`expr -> $name`** instead of mixing `=` into shadow updates.

**Window buffer** (ring buffer of raw values) — **as implemented today**:

```
@[5](ma5 = get_ma(prices, 5))
```

Allocates a ring buffer of length 5. The variable `ma5` is computed each frame and stored.

**Scalar accumulator** (scan/fold state):

```
@[total = 0.0](total_vol = $total + volumes)
```

Allocates a single persistent scalar initialized to `0.0`. Updated every frame.

### 9.3 Shadow Reference: `$`

`$var` accesses a declared state container:

```
$ma5          // current value of the ma5 state
$total        // current accumulator value
```

The `$` prefix is **mandatory** when referencing stateful variables. Without `@[...]` declaration, using `$var` is a compile error.

### 9.4 Retired History Probe: `[<<, n]`

The `$state[<<, n]` spelling belonged to the old M6 history-probe draft and is
not active syntax. Future history access must be introduced through a revised
state-topology fixture instead of reviving this selector.

### 9.5 Pulse Frame Lock

**Core invariant:** Within a single pulse frame (one execution of the closure), all `$var` reads return the **same snapshot value**, regardless of how many times they appear in the expression.

```
result = $ma5 * $ma5    // guaranteed: both reads return identical values
```

This prevents non-deterministic "time-tearing" when external feeds update mid-computation.

**Hot pull exception:** `(<< @resource)` bypasses frame lock, performing a live read.

### 9.6 Anonymous Ledger (Implicit Hoisting)

Developers may freely scatter `@[...]` declarations anywhere in their code. The compiler automatically:

1. Extracts all state declarations via **implicit hoisting**
2. Allocates them in a **contiguous memory region** (the anonymous ledger)
3. Redirects all `$var` references to fixed offsets within this region

This provides "write messy, compile clean" ergonomics. A `styio audit --fix` tool can rewrite source to move declarations to file headers.

### 9.7 Schema (Explicit Ledger)

For large systems, explicit schemas provide full control:

```
# GlobalState := schema {
    @[1024] price_history
    @pulse  total_count
    @meta   strategy_id
}
```

Local variables can mount to schema slots for cross-flow sharing, instant serialization, and hot-restart state recovery.

---

## 10. Stream Synchronization

### 10.1 Aligned Sync (Zip): `&`

```
@binance >> #(p) & @okx >> #(p_okx) => {
    arbitrage_gap = p - p_okx
}
```

Both streams must deliver a pulse before the closure executes. The trigger frequency is `min(freq_A, freq_B)`.

Optional tolerance window:

```
@binance >> #(p) &[5ms] @okx >> #(p_okx) => { ... }
```

### 10.2 Snapshot Pull: `<< @resource`

```
@binance >> #(p) => {
    @[p_okx] << @okx{"BTC"}
    gap = p - $p_okx
}
```

The `@[p_okx] << @okx` declaration establishes a **background listener**. The main flow (`@binance`) drives execution; `$p_okx` provides the latest available snapshot from OKX. Frame lock applies to `$p_okx`.

Inline immediate pull (no state declaration, live read):

```
gap = p - (<- @okx{"BTC"})
```

### 10.3 Synchronization Summary

| Mode | Syntax | Trigger | Use Case |
|------|--------|---------|----------|
| Zip | `A >> #(a) & B >> #(b)` | Both arrive | Atomic cross-exchange arbitrage |
| Snapshot | `@[v] << @res` + `$v` | Main flow only | Cross-frequency reference |
| Immediate Pull | `(<- @res)` | On-demand | One-shot live sampling |

---

## 11. Selector Operators: `[mode, arg]`

Square brackets serve as a **contextual transformer**, not just an indexer.

### 11.1 Static Indexing and Slicing

```
a[0]        // element at index 0
a[-1]       // last element (only for arrays in memory)
a[0..5]     // slice: indices 0 to 4
a[2...]     // slice: index 2 to end
```

### 11.2 Retired Guard Selector: `[?, cond]`

The postfix guard selector was an early draft and is no longer active syntax.
Use `?(cond) => value | fallback` for value selection, or normal `?(cond) => { ... }`
blocks for statement-level control.

### 11.3 Retired Equality Probe: `[?=, val]`

The postfix equality probe was retired with the guard selector. Use `?=` match
blocks for equality-style branching.

### 11.4 Plugin Operators: `[op, n]`

```
prices[avg, 20]    // 20-period moving average (compiler intrinsic)
prices[max, 14]    // 14-period rolling maximum
prices[std, 20]    // 20-period standard deviation
```

These are **compiler intrinsics** — the compiler inlines optimized algorithms (O(1) sliding sum, monotonic queue, Welford's algorithm) directly into the generated code.

### 11.5 Retired History Probe: `[<<, n]`

The `$state[<<, n]` postfix history selector is not an active milestone syntax.
Future history access must re-enter with a revised selector or state-topology
fixture instead of reusing the old M6 spelling.

Historical examples remain provenance only in archived milestone docs.

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

**Type formatting rules** (applies to `>_()`, scalar `-> @stdout`, and iterable `>> @stdout` after serialization):

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

In debug mode, `@` values carry tainted metadata:

```
last_signal ?? reason    // "DataSource(@binance) timeout at 14:22:05.123"
```

The `??` operator extracts the diagnostic context from a tainted `@`.

### 13.4 Guard-based Recovery

```
safe_price = price | $last_valid_price    // fallback if price carries runtime absence
```

The `|` operator provides a fallback value when the left side carries runtime absence.

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

- Scans scattered `@[...]` declarations
- Generates explicit `schema` block at file header
- Reformats source according to Styio style guide

---

## Appendix A: Consultant's Additional Thoughts

### A.1 Reconciling Design with Existing Codebase

The current C++ compiler implementation already has a rich token system, parser, AST, IR, and LLVM codegen. However, significant features from the Gemini design discussion are not yet implemented:

- **Reserved wave tokens** (`<~`, `~>`) already exist at the lexer level but have no active grammar production
- **State containers** (`@[len]`, `$var`) require a new state analysis pass
- **Pulse Frame Lock** needs runtime infrastructure in the JIT executor
- **Cross-stream sync** (`&`, `<< @res`) requires a concurrency model in the IR

**Recommended implementation order:**
1. Keep `?(cond) => value | fallback` and `?(cond) => { ... } | { ... }` as the active guard forms
2. Extend the Lexer with new token types for `@[`, `$`
3. Add AST nodes: `StateDeclNode`, `StateRefNode`, `StreamZipNode`
4. Implement state hoisting in the analyzer (anonymous ledger)
5. Extend LLVM codegen for state containers (stack-allocated ring buffers)
6. Add concurrency primitives for stream synchronization

### A.2 Open Design Questions

1. **`>>` ambiguity resolution:** The parser must distinguish between pipe (`source >> consumer`), continue (`>>` as standalone statement), and stride selector (`[>>, 2]`). The current implementation already handles `>>` as `Iterate` — extending this to multi-meaning requires careful lookahead logic.

2. **`@` overload risk:** `@` remains overloaded as a resource prefix, state prefix, standard-stream prefix, and runtime absence marker. Source-level bare `@` has been retired from active syntax to reduce ambiguity.

3. **Scan vs. Window unification:** Both `@[n](var = expr)` (window) and `@[var = init](expr)` (scan/accumulator) use the `@[...]` syntax. The compiler must distinguish them by whether the bracket contains a number (window size) or an assignment (accumulator init). This is parseable but should be clearly specified.

4. **Cross-platform builds:** The current CMakeLists.txt hardcodes Linux paths. Windows and macOS support need platform-conditional toolchain detection.

### A.3 Performance Considerations

The **Pulse Frame Lock** design is elegant but may introduce measurable overhead in ultra-high-frequency scenarios (>100k ticks/sec). Consider:
- A compile-time optimization that detects when frame lock is unnecessary (single `$var` read, no aliasing)
- An `unsafe` annotation to opt out of frame lock for latency-critical inner loops
- Hardware-level atomic snapshot using `LOCK CMPXCHG` or ARM `LDXR/STXR` for multi-threaded shadow updates
