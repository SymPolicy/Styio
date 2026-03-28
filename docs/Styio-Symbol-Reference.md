# Styio Symbol Reference

**Version:** 1.0-draft  
**Date:** 2026-03-28

This document serves as the definitive lookup table for all symbols in Styio. It is the primary reference for implementing `enum class TokenKind` in the C++ lexer.

---

## 1. Resource & State Identifiers

| Symbol | Name | C++ Token Kind | Physical Semantics |
|--------|------|----------------|-------------------|
| `@` | Undefined / Resource Anchor | `TOK_AT` | **Alone:** honest absence (Undefined). **Before `[`:** state container declaration. **Before identifier + `{`/`(`:** resource with protocol. |
| `$` | State Reference / Capture | `TOK_DOLLAR` | **Before identifier:** read from shadow buffer. **Before `(`:** capture list in function decl. **Before string:** format string. |

---

## 2. Data Flow Operators

| Symbol | Name | C++ Token Kind | Semantics | Example |
|--------|------|----------------|-----------|---------|
| `>>` | Pipe / Iterate | `TOK_PIPE` | Push pulse from source into consumer | `prices >> #(p) => { ... }` |
| `->` | Forward / Redirect | `TOK_ARROW_RIGHT` | Redirect data to a physical destination | `ma5 -> @database(...)` |
| `<-` | Acquire Handle | `TOK_ARROW_LEFT` | Extract live handle from resource | `f <- @file{"data.txt"}` |
| `<<` | Write / Shift-Back | `TOK_SHIFT_BACK` | **In `[<<, n]`:** history probe. **Standalone:** write to resource. **In `(<< @res)`:** instant pull. |
| `<\|` | Yield / Return | `TOK_YIELD` | Push value out of block (expression return) | `<\| x * x` |
| `>_` | I/O Buffer / Print | `TOK_IO_BUF` | Side-effect output to stdout or system buffer | `>_("hello")` |

---

## 3. Wave Operators (Conditional Routing)

| Symbol | Name | C++ Token Kind | Direction | Semantics |
|--------|------|----------------|-----------|-----------|
| `<~` | Conditional Merge | `TOK_WAVE_LEFT` | ← (pull toward receiver) | `val = (cond) <~ a \| b` |
| `~>` | Conditional Dispatch | `TOK_WAVE_RIGHT` | → (push toward target) | `(cond) ~> target \| @` |
| `\|` | Fallback / Else | `TOK_PIPE_SINGLE` | — | Else-branch for wave operators; fallback for `@` recovery |

---

## 4. Guard & Selector Operators

| Syntax | Name | Context | Semantics |
|--------|------|---------|-----------|
| `[?, cond]` | Predicate Guard | Postfix on any value | Returns value if `cond` is true, else `@` |
| `[?=, val]` | Equality Probe | Postfix on any value | Returns value if `value == val`, else `@` |
| `[<<, n]` | History Probe | Postfix on `$var` | Returns value from `n` pulses ago |
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
| `?(expr)` | Guard / Valve | `TOK_QUESTION` + `(` | Conditional filter on stream pipeline |
| `=>` | Map / Then | `TOK_FAT_ARROW` | Connects pattern/param to result/body |
| `^` ... `^^^^` | Break | `BREAK_TOKEN(n)` | Exit `n` levels of enclosing loops |
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
| `-` | Subtract | 702 |
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

## 9. Diagnostic

| Symbol | Name | Semantics |
|--------|------|-----------|
| `??` | Diagnostic Extract | Extracts reason/metadata from a tainted `@` value |

---

## 10. Lexer Disambiguation Quick Reference

| Input | Resolution |
|-------|------------|
| `@` alone | Undefined value |
| `@[` | State container declaration prefix |
| `@ident{...}` | Resource with explicit protocol |
| `@{...}` or `@(...)` | Anonymous resource (auto-detect) |
| `$ident` | State reference |
| `$(...)` | Capture list (function context) |
| `$"..."` | Format string |
| `>>` after expr, before `#`/`{`/ident | Pipe operator |
| `>>` as standalone statement | Continue (1 level) |
| `>>>` standalone | Continue (2 levels) |
| `[<<, n]` inside brackets | History probe selector |
| `(<< @res)` in parens | Instant pull |
| `<~` | Wave merge (always 2-char token) |
| `~>` | Wave dispatch (always 2-char token) |
| `^` contiguous | Break (count = number of `^`) |
| `^^ ^^` with space | **Illegal** — two separate breaks, rejected by parser |

---

## Appendix: Consultant's Notes

### Symbol Density Mitigation

Styio has ~40 distinct symbolic constructs. This is comparable to APL/J but with clearer visual grouping due to the "family" structure:

- **`>` family:** `>`, `>>`, `>>>`, `>=`, `>_`, `~>`
- **`<` family:** `<`, `<<`, `<=`, `<-`, `<|`, `<~`, `<:`
- **`@` family:** `@`, `@[`, `@ident{...}`
- **`$` family:** `$var`, `$(...)`, `$"..."`
- **`?` family:** `?`, `?=`, `?(...)`, `??`

The lexer should process these families using a **trie-based dispatch** after reading the first character. This avoids the combinatorial explosion of a flat switch-case.

### Recommended C++ Token Enum Extension

The existing `StyioOpType` enum should be extended with:

```cpp
TOK_WAVE_LEFT,       // <~
TOK_WAVE_RIGHT,      // ~>
TOK_AT_BRACKET,      // @[
TOK_DOLLAR_IDENT,    // $identifier
TOK_DOLLAR_PAREN,    // $(
TOK_DOLLAR_STRING,   // $"..."
TOK_DBQUESTION,      // ??
TOK_AMPERSAND,       // & (stream zip)
TOK_BREAK(int n),    // ^...^ with depth
TOK_CONTINUE(int n), // >>...> with depth
```
