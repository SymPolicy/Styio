# Styio Language Design Specification

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
| **Honest Missing** | The native `@` (Undefined) propagates algebraically through all operations: `10 + @ = @`. No silent defaults, no hidden NaN traps. |
| **Thick Library, Thin Artifact** | Development uses a rich standard library with protocol detection and AI-assisted probing. Production builds perform dead-code elimination to produce minimal binaries. |

### 1.2 Compiler Toolchain

- **Language:** C++20
- **Backend:** LLVM 18+ (IRBuilder + ORC JIT)
- **Parser Strategy:** Hand-written recursive descent, LL(n) with lookahead
- **Dependencies:** LLVM, ICU (Unicode support)

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
- Integer literals default to `i32`
- Floating-point literals default to `f64`

### 3.3 Type Annotations

Types are annotated with `:` on both parameters and return values:

```
# add : f32 = (a: f32, b: f32) => a + b
```

- `add : f32` — return type is `f32`
- `a: f32` — parameter type
- `:` always binds a **type** to its left-hand identifier

### 3.4 The Undefined Type: `@`

`@` is a first-class value representing **honest absence**. It is not `null`, not `0`, not `NaN` — it is the explicit admission that data does not exist.

**Propagation rules:**
- `x + @ = @` for any arithmetic operation
- `x && @ = @` for any logical operation
- `@` short-circuits through all expressions until explicitly intercepted

**Diagnostic tainting (debug mode):**
In debug builds, `@` carries metadata (reason code, source location) enabling root-cause tracing via `.reason()`.

---

## 4. Functions

### 4.1 Definition Syntax

Functions are declared with `#`:

```
# add := (a, b) => { <| a + b }    // block form with explicit yield
# add := (a, b) => a + b            // expression form (implicit yield)
# add := (a: i32, b: i32) => a + b  // with type annotations
# add : i32 = (a: i32, b: i32) => a + b  // with return type
```

### 4.2 Anonymous Closures

Used within stream pipes:

```
prices >> #(p) => { <| p * 2 }
```

`#(p)` binds the current pulse to the local name `p`.

### 4.3 Context Capture with `$(...)`

Functions can explicitly capture external variables by reference:

```
trade $(bal, is_open) := my_strategy[?, is_open][?, bal > 100]
```

The `$(...)` list declares a **reactive binding** — the function re-evaluates whenever captured variables change.

---

## 5. Control Flow

Styio's control flow is entirely **symbol-driven** and **expression-oriented**.

### 5.1 Pattern Matching: `?=`

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

### 5.2 Infinite Loop

```
[...] => { /* body */ }
```

`[...]` is an infinite pulse generator. The closure executes indefinitely.

### 5.3 Conditional Loop (While-equivalent)

```
[...] ?(expr) >> { /* body */ }
```

- `[...]` — infinite generator
- `?(expr)` — guard / valve: only passes pulses when `expr` is truthy
- `>>` — pipe into closure

### 5.4 Collection Iteration (For-each-equivalent)

```
[1, 2, 3] >> #(item) => { /* body */ }
```

The collection becomes a finite pulse source. Each element is bound to `item`.

### 5.5 Break: `^^^^` (Variable Length)

```
^^^^    // break out of 4 nested loops
^^      // break out of 2
^       // break out of 1
```

Rules:
- `^` characters must be **contiguous** (no spaces)
- `^^ ^^` is **illegal** — the compiler rejects it
- Depth exceeding current loop nesting produces a compile-time error

### 5.6 Continue: `>>` (Variable Length, ≥2)

```
>>      // skip current iteration (1 level)
>>>     // skip 2 levels
>>>>    // skip 3 levels
```

The base continue is 2 characters (`>>`). Each additional `>` skips one more nesting level. Context distinguishes continue from pipe: continue appears as a **standalone statement** (not connecting source to consumer).

### 5.7 Yield / Return: `<|`

```
# square := (x) => { <| x * x }
```

`<|` pushes a value out of the current block. When used in control flow that is part of an assignment, it produces the value for the enclosing expression.

When multiple branches yield (e.g., in `?=`), the compiler generates LLVM `phi` nodes at the merge point.

---

## 6. Wave Operators: Conditional Routing

Wave operators replace ternary expressions and if/else chains with **directional logic flow**.

### 6.1 Conditional Merge: `<~`

```
val = (a > b) <~ a | b
```

Read as: "If condition holds, wave toward `a`; otherwise fall back to `|` value `b`."

### 6.2 Conditional Dispatch: `~>`

```
(signal) ~> order_logic(p) | @
```

Read as: "If signal is truthy, dispatch pulse to `order_logic`; otherwise route to `@` (void)."

### 6.3 Visual Semantics

| Symbol | Direction | Meaning |
|--------|-----------|---------|
| `<~` | leftward (merge) | Pull value toward the receiver based on condition |
| `~>` | rightward (dispatch) | Push pulse toward a target based on condition |
| `\|` | fallback | The else-branch in both merge and dispatch |

---

## 7. Resource System

### 7.1 Resource Identifiers: `@`

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

### 7.2 Handle Acquisition: `<-`

```
f <- @file{"readme.txt"}
```

`<-` extracts a live handle (file descriptor, socket, cursor) from a resource.

### 7.3 Reading: `>>`

```
f >> #(chunk: [byte; 4096]) => { buf += chunk }
```

### 7.4 Writing: `<<`

```
"Hello Styio" << f
```

### 7.5 Lifecycle: Scope-based RAII

Resources are automatically released when their enclosing scope ends. The compiler inserts cleanup code at every exit path (including `^^` breaks and `<|` returns).

### 7.6 Persistence via Redirection: `->`

```
ma5 -> @database("redis://localhost/ma5_cache")
```

`->` redirects a value's storage destination. The runtime asynchronously syncs to the target resource.

---

## 8. State Management

### 8.1 The Problem

Stream processing requires **memory across pulses**. A simple local variable resets every frame. Styio solves this with explicit state containers.

### 8.2 State Container: `@[...]`

**Window buffer** (ring buffer of raw values):

```
@[5](ma5 = get_ma(prices, 5))
```

Allocates a ring buffer of length 5. The variable `ma5` is computed each frame and stored.

**Scalar accumulator** (scan/fold state):

```
@[total = 0.0](total_vol = $total + volumes)
```

Allocates a single persistent scalar initialized to `0.0`. Updated every frame.

### 8.3 Shadow Reference: `$`

`$var` accesses a declared state container:

```
$ma5          // current value of the ma5 state
$ma5[<<, 1]   // value from 1 pulse ago (history probe)
$total        // current accumulator value
```

The `$` prefix is **mandatory** when referencing stateful variables. Without `@[...]` declaration, using `$var` is a compile error.

### 8.4 History Probe: `[<<, n]`

```
$ma5[<<, 1]    // previous frame's ma5
$ma5[<<, 5]    // 5 frames ago
```

Only valid on `$`-prefixed state references. The compiler verifies that the declared buffer length ≥ `n`.

### 8.5 Pulse Frame Lock

**Core invariant:** Within a single pulse frame (one execution of the closure), all `$var` reads return the **same snapshot value**, regardless of how many times they appear in the expression.

```
result = $ma5 * $ma5    // guaranteed: both reads return identical values
```

This prevents non-deterministic "time-tearing" when external feeds update mid-computation.

**Hot pull exception:** `(<< @resource)` bypasses frame lock, performing a live read.

### 8.6 Anonymous Ledger (Implicit Hoisting)

Developers may freely scatter `@[...]` declarations anywhere in their code. The compiler automatically:

1. Extracts all state declarations via **implicit hoisting**
2. Allocates them in a **contiguous memory region** (the anonymous ledger)
3. Redirects all `$var` references to fixed offsets within this region

This provides "write messy, compile clean" ergonomics. A `styio audit --fix` tool can rewrite source to move declarations to file headers.

### 8.7 Schema (Explicit Ledger)

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

## 9. Stream Synchronization

### 9.1 Aligned Sync (Zip): `&`

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

### 9.2 Snapshot Pull: `<< @resource`

```
@binance >> #(p) => {
    @[p_okx] << @okx{"BTC"}
    gap = p - $p_okx
}
```

The `@[p_okx] << @okx` declaration establishes a **background listener**. The main flow (`@binance`) drives execution; `$p_okx` provides the latest available snapshot from OKX. Frame lock applies to `$p_okx`.

Inline instant pull (no state declaration, live read):

```
gap = p - (<< @okx{"BTC"})
```

### 9.3 Synchronization Summary

| Mode | Syntax | Trigger | Use Case |
|------|--------|---------|----------|
| Zip | `A >> #(a) & B >> #(b)` | Both arrive | Atomic cross-exchange arbitrage |
| Snapshot | `@[v] << @res` + `$v` | Main flow only | Cross-frequency reference |
| Instant Pull | `(<< @res)` | On-demand | One-shot live sampling |

---

## 10. Selector Operators: `[mode, arg]`

Square brackets serve as a **contextual transformer**, not just an indexer.

### 10.1 Static Indexing and Slicing

```
a[0]        // element at index 0
a[-1]       // last element (only for arrays in memory)
a[0..5]     // slice: indices 0 to 4
a[2...]     // slice: index 2 to end
```

### 10.2 Guard Selector: `[?, cond]`

```
price[?, volume > 1000]    // returns price if condition holds, else @
```

### 10.3 Equality Probe: `[?=, val]`

```
a[?=, 3]    // returns a if a == 3, else @
```

### 10.4 Plugin Operators: `[op, n]`

```
prices[avg, 20]    // 20-period moving average (compiler intrinsic)
prices[max, 14]    // 14-period rolling maximum
prices[std, 20]    // 20-period standard deviation
```

These are **compiler intrinsics** — the compiler inlines optimized algorithms (O(1) sliding sum, monotonic queue, Welford's algorithm) directly into the generated code.

### 10.5 History Probe: `[<<, n]`

```
$ma5[<<, 1]    // previous value of ma5 state
```

Only valid on state references (`$`-prefixed variables).

---

## 11. I/O and Side Effects

### 11.1 Standard Output: `>_`

```
>_("Hello, Styio!")
>_(variable)
```

### 11.2 I/O Buffer: `>_` (stream context)

In stream contexts, `>_` writes to the system's buffered output channel.

### 11.3 Format Strings: `$`

```
>_($"Price is {p}, Volume is {v}")
```

---

## 12. Error Handling Philosophy

### 12.1 Fail-Fast for Structural Errors

If a resource schema mismatch is detected (e.g., accessing a non-existent database column), the program terminates immediately at connection time — **before** the first data pulse. No silent degradation.

### 12.2 Algebraic Propagation for Data Errors

Missing data within a stream becomes `@`, which propagates through all downstream computations. The pipeline naturally "goes silent" rather than producing incorrect results.

### 12.3 Diagnostic Tracing

In debug mode, `@` values carry tainted metadata:

```
last_signal ?? reason    // "DataSource(@binance) timeout at 14:22:05.123"
```

The `??` operator extracts the diagnostic context from a tainted `@`.

### 12.4 Guard-based Recovery

```
safe_price = price | $last_valid_price    // fallback if price is @
```

The `|` operator provides a fallback value when the left side is `@`.

---

## 13. Compilation Modes

### 13.1 Development Mode (JIT)

- Full standard library loaded
- AI-assisted protocol probing enabled
- LLVM ORC JIT for instant execution
- Diagnostic tainting active

### 13.2 Strict Mode (AOT, `styio build --strict`)

- All types must be explicitly annotated
- All resource protocols must be specified
- Intent-aware dead code elimination
- Output: minimal native binary or WebAssembly module

### 13.3 Audit Mode (`styio audit --fix`)

- Scans scattered `@[...]` declarations
- Generates explicit `schema` block at file header
- Reformats source according to Styio style guide

---

## Appendix A: Consultant's Additional Thoughts

### A.1 Reconciling Design with Existing Codebase

The current C++ compiler implementation already has a rich token system, parser, AST, IR, and LLVM codegen. However, significant features from the Gemini design discussion are not yet implemented:

- **Wave operators** (`<~`, `~>`) need new token types and AST nodes
- **State containers** (`@[len]`, `$var`) require a new state analysis pass
- **Pulse Frame Lock** needs runtime infrastructure in the JIT executor
- **Cross-stream sync** (`&`, `<< @res`) requires a concurrency model in the IR

**Recommended implementation order:**
1. Extend the Lexer with new token types for `<~`, `~>`, `@[`, `$`
2. Add AST nodes: `WaveExprNode`, `StateDeclNode`, `StateRefNode`, `StreamZipNode`
3. Implement state hoisting in the analyzer (anonymous ledger)
4. Extend LLVM codegen for state containers (stack-allocated ring buffers)
5. Add concurrency primitives for stream synchronization

### A.2 Open Design Questions

1. **`>>` ambiguity resolution:** The parser must distinguish between pipe (`source >> consumer`), continue (`>>` as standalone statement), and stride selector (`[>>, 2]`). The current implementation already handles `>>` as `Iterate` — extending this to multi-meaning requires careful lookahead logic.

2. **`@` overload risk:** `@` serves as undefined value, resource prefix, and state container prefix. Consider whether the parser can always unambiguously resolve these based on context (`@` alone = undefined, `@ident{...}` = resource, `@[...]` = state).

3. **Scan vs. Window unification:** Both `@[n](var = expr)` (window) and `@[var = init](expr)` (scan/accumulator) use the `@[...]` syntax. The compiler must distinguish them by whether the bracket contains a number (window size) or an assignment (accumulator init). This is parseable but should be clearly specified.

4. **Cross-platform builds:** The current CMakeLists.txt hardcodes Linux paths. Windows and macOS support need platform-conditional toolchain detection.

### A.3 Performance Considerations

The **Pulse Frame Lock** design is elegant but may introduce measurable overhead in ultra-high-frequency scenarios (>100k ticks/sec). Consider:
- A compile-time optimization that detects when frame lock is unnecessary (single `$var` read, no aliasing)
- An `unsafe` annotation to opt out of frame lock for latency-critical inner loops
- Hardware-level atomic snapshot using `LOCK CMPXCHG` or ARM `LDXR/STXR` for multi-threaded shadow updates
