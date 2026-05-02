# Styio Symbol Reference

**Purpose:** 各符号的 **lexer token 名与物理含义速查表**；完整语义与章节论证见 [`Styio-Language-Design.md`](./Styio-Language-Design.md)。实现 `enum class TokenKind` 时以本文与 EBNF 对照。

**Last updated:** 2026-04-24

**Version:** 1.0-draft  
**Date:** 2026-03-28

This document serves as the definitive lookup table for all symbols in Styio. It is the primary reference for implementing `enum class TokenKind` in the C++ lexer.

---

## 1. Resource & State Identifiers

| Symbol | Name | C++ Token Kind | Physical Semantics |
|--------|------|----------------|-------------------|
| `@` | Undefined / Resource Anchor | `TOK_AT` | **Alone:** honest absence (Undefined). **Before `[`:** state container declaration. **Before identifier + `{`/`(`:** resource with protocol. |
| `@stdout` | Standard Output | `TOK_AT` + `NAME("stdout")` | Built-in write-only stream resource (fd 1). Scalar write: `expr -> @stdout`; iterable write: `items >> @stdout`. |
| `@stderr` | Standard Error | `TOK_AT` + `NAME("stderr")` | Built-in write-only stream resource (fd 2, unbuffered). Scalar write: `expr -> @stderr`; iterable write: `items >> @stderr`. |
| `@stdin` | Standard Input | `TOK_AT` + `NAME("stdin")` | Built-in read-only stream resource (fd 0). Iterate via `@stdin >> #(line) => {...}`. Symbolic definition forms include `{ <\|[>_] }`, `{ <\|(>_) }`, and expanded `{ <\| <- [>_] }`. Legacy `(<< @stdin)` is compatibility-only, not canonical design spelling. |
| `$` | State Reference / Capture | `TOK_DOLLAR` | **Before identifier:** read from shadow buffer. **Before `(`:** capture list in function decl. **Before string:** format string. |

---

## 2. Data Flow Operators

| Symbol | Name | C++ Token Kind | Semantics | Example |
|--------|------|----------------|-----------|---------|
| `>>` | Pipe / Iterate / Resource-Write Shorthand | `TOK_PIPE` | **Before iterator tail:** push pulse from source into consumer. **Before resource atom (`@file{...}`, `@stdout`, `@stderr`, `@stdin`)**: parse as `resource_write` shorthand. **Before `[>_]`, `@stdout`, or `@stderr`:** iterable text serialization only; plain strings must use `->` or explicit `text.lines() >> ...`. `@stdin` remains semantically read-only. | `prices >> #(p) => { ... }`, `items >> @stdout`, `text.lines() >> [>_]` |
| `->` | Forward / Redirect | `TOK_ARROW_RIGHT` | Redirect data to a physical destination | `ma5 -> @database(...)` |
| `<-` | Acquire / Pull | `TOK_ARROW_LEFT` | Extract or acquire from a resource; used in expanded stdin symbolic definition as `<\| <- [>_]`. | `f <- @file{"data.txt"}` |
| `<<` | Write / Shift-Back | `TOK_SHIFT_BACK` | **Retired in `[<<, n]`:** old history probe spelling. **Legacy `(<< @res)`:** compatibility instant pull only; avoid for new read/pull design. |
| `<\|` | Return / One-Shot Apply | `YIELD_PIPE` | **Statement start:** return value from block. **Infix:** left-associative one-shot resume/apply; `f <\| a <\| b == f(a)(b)`. | `<\| x * x`, `f <\| 1` |
| `\|<\|` | Inline Return | `RETURN_PIPE` | One-line return form, ended by `\|;`. | `...; \|<\| result \|;` |
| `\|;` | Statement Separator | `PIPE_SEMICOLON` | Explicit separator for compressed one-line blocks. | `x = 1; \|<\| x \|;` |
| `>_` | Terminal Device | `TOK_IO_BUF` | **As statement:** `>_(expr)` prints to stdout (legacy). **As value:** first-class terminal device handle. Canonical symbolic spelling is `[>_]`; `(>_)` remains compatibility. | `>_("hello")`, `<\|(>_)` |

---

## 3. Reserved Wave Tokens

| Symbol | Name | C++ Token Kind | Direction | Semantics |
|--------|------|----------------|-----------|-----------|
| `<~` | Reserved Wave Left | `TOK_WAVE_LEFT` | Reserved | Tokenized for future reassignment; parser rejects active use |
| `~>` | Reserved Wave Right | `TOK_WAVE_RIGHT` | Reserved | Tokenized for future reassignment; parser rejects active use |
| `\|` | Fallback / Else | `TOK_PIPE_SINGLE` | — | Else-branch for guard conditionals; fallback for runtime absence recovery |

---

## 4. Guard & Selector Operators

| Syntax | Name | Context | Semantics |
|--------|------|---------|-----------|
| `[?, cond]` | Retired Predicate Guard | Inactive old milestone syntax | Use `?(cond) => value \| fallback` or `?(cond) => { ... }` |
| `[?=, val]` | Retired Equality Probe | Inactive old milestone syntax | Use `?=` match blocks |
| `[<<, n]` | Retired History Probe | Inactive old milestone syntax | Future history access must re-enter through a revised selector/state-topology fixture |
| `[avg, n]` | Moving Average | Postfix on stream | Compiler intrinsic: O(1) sliding sum |
| `[max, n]` | Rolling Maximum | Postfix on stream | Compiler intrinsic: monotonic queue |
| `[min, n]` | Rolling Minimum | Postfix on stream | Compiler intrinsic: monotonic queue |
| `[std, n]` | Rolling Std Dev | Postfix on stream | Compiler intrinsic: Welford's algorithm |
| `[rsi, n]` | RSI Oscillator | Postfix on stream | Compiler intrinsic: Wilder SMMA |

---

## 5. Control Flow Symbols

| Symbol | Name | C++ Token Kind | Semantics |
|--------|------|----------------|-----------|
| `?=` | Pattern Match | `TOK_MATCH` | Trigger pattern matching block |
| `?(expr)` | Guard / Paren marker | `TOK_QUEST` + `(` | **As an expression:** `?(expr) => value \| fallback`. **As a statement:** `?(expr) => { ... }` with optional fallback `\| { ... }`. **After `[...] >>`:** `?(expr) =>` → conditioned loop (`InfiniteLoopAST`). |
| `=>` | Map / Then | `TOK_FAT_ARROW` | Connects pattern/param to result/body |
| `^` ... `^^^^` | Break | `BREAK_TOKEN` | Exit the nearest enclosing loop; count is normalized to 1 |
| `>>` ... `>>>>` | Continue | `CONTINUE_TOKEN(n)` | Skip to next iteration, `n-1` levels up |
| `[...]` | Infinite Generator | `[` + `TOK_ELLIPSIS` + `]` | Produces infinite pulse stream |
| `&` | Stream Zip | `TOK_AMPERSAND` | Align two streams (both must deliver) |

---

## 6. Assignment & Binding

| Symbol | Name | C++ Token Kind | Semantics |
|--------|------|----------------|-----------|
| `=` | Assignment | `TOK_ASSIGN` | Bind value (overwrite per pulse in stream context) |
| `:=` | State Binding | `TOK_BIND` | Establish reactive/persistent binding |
| `+=` | Aggregate Assign | `TOK_PLUS_ASSIGN` | Accumulate (semi-ring fold in stream context) |
| `-=` | Subtract Assign | `TOK_MINUS_ASSIGN` | Subtract-accumulate |
| `*=` | Multiply Assign | `TOK_STAR_ASSIGN` | Multiply-accumulate |
| `/=` | Divide Assign | `TOK_SLASH_ASSIGN` | Divide-accumulate |

---

## 7. Type & Definition

| Symbol | Name | Semantics |
|--------|------|-----------|
| `#` | Function Prefix | Introduces function/closure definition |
| `:` | Type Annotation | Binds a type to identifier (`a: i32`, `# f : f32 = ...`) |
| `_` | Wildcard | Default/catch-all in pattern matching |

---

## 8. Arithmetic & Logic

| Symbol | Name | Precedence |
|--------|------|------------|
| `**` | Power | 704 |
| `*` | Multiply | 703 |
| `/` | Divide | 703 |
| `%` | Modulo | 703 |
| `+` | Add | 702 |
| `-` | Subtract / Numeric Sign | 702 |
| `>` | Greater Than | 502 |
| `<` | Less Than | 502 |
| `>=` | Greater or Equal | 502 |
| `<=` | Less or Equal | 502 |
| `==` | Equal | 501 |
| `!=` | Not Equal | 501 |
| `&&` | Logical AND | 401 |
| `\|\|` | Logical OR | 400 |
| `!` | Logical NOT | 999 (unary) |

---

## 9. Diagnostic & Channel Selection

| Symbol | Name | Semantics |
|--------|------|-----------|
| `??` | Diagnostic Extract | Extracts reason/metadata from a tainted `@` value |
| `!(expr)` | Channel Selector | **Before `-> ( >_ )`:** selects stderr channel (fd 2) instead of stdout (fd 1). In other contexts, `!` remains logical NOT. |

---

## 10. Lexer Disambiguation Quick Reference

| Input | Resolution |
|-------|------------|
| `@` alone | Undefined value |
| `@[` | State container declaration prefix |
| `@ident{...}` | Resource with explicit protocol |
| `@{...}` or `@(...)` | Anonymous resource (auto-detect) |
| `@stdout`, `@stderr`, `@stdin` | Standard stream resource atom (compiler-recognized directly; no user-authored wrapper definition required) |
| `$ident` | State reference |
| `$(...)` | Capture list (function context) |
| `$"..."` | Format string |
| `>>` after expr, before `#`/`{`/ident | Pipe operator |
| `>>` as standalone statement | Continue (1 level) |
| `>>>` standalone | Continue (2 levels) |
| `[<<, n]` inside brackets | Retired history probe selector; not active acceptance syntax |
| `(<- @res)` in parens | Immediate pull |
| `(<< @res)` in parens | Legacy compatibility pull |
| `<~` | Reserved token (always 2-char token); parser rejects active use |
| `~>` | Reserved token (always 2-char token); parser rejects active use |
| `^` contiguous | Break (count ignored; always nearest loop) |
| `^^ ^^` with space | **Illegal** — two separate breaks, rejected by parser |

---

## Appendix: Consultant's Notes

### Symbol Density Mitigation

Styio has ~40 distinct symbolic constructs. This is comparable to APL/J but with clearer visual grouping due to the "family" structure:

- **`>` family:** `>`, `>>`, `>>>`, `>=`, `>_`, `~>` (reserved)
- **`<` family:** `<`, `<<`, `<=`, `<-`, `<|`, `<~` (reserved), `<:`
- **`|` family:** `|`, `||`, `|]`, `|<|`, `|;`
- **`@` family:** `@`, `@[`, `@ident{...}`
- **`$` family:** `$var`, `$(...)`, `$"..."`
- **`?` family:** `?`, `?=`, `?(...)`, `??`

The lexer should process these families using a **trie-based dispatch** after reading the first character. This avoids the combinatorial explosion of a flat switch-case.

### Recommended C++ Token Enum Extension

The existing `StyioOpType` enum should be extended with:

```cpp
TOK_WAVE_LEFT,       // <~ reserved
TOK_WAVE_RIGHT,      // ~> reserved
TOK_AT_BRACKET,      // @[
TOK_DOLLAR_IDENT,    // $identifier
TOK_DOLLAR_PAREN,    // $(
TOK_DOLLAR_STRING,   // $"..."
TOK_DBQUESTION,      // ??
TOK_AMPERSAND,       // & (stream zip)
TOK_BREAK,           // ^...^ normalized to nearest-loop break
TOK_CONTINUE(int n), // >>...> with depth
```
