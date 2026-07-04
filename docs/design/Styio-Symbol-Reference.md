# Styio Symbol Reference

**Purpose:** 各符号的 **lexer token 名与物理含义速查表**；完整语义与章节论证见 [`Styio-Language-Design.md`](./Styio-Language-Design.md)。实现 `enum class TokenKind` 时以本文与 EBNF 对照。

**Last updated:** 2026-05-09

**Version:** 1.0-draft  
**Date:** 2026-03-28

This document serves as the definitive lookup table for all symbols in Styio. It is the primary reference for implementing `enum class TokenKind` in the C++ lexer.

---

## 1. Resource & State Identifiers

| Symbol | Name | C++ Token Kind | Physical Semantics |
|--------|------|----------------|-------------------|
| `@` | Resource Anchor | `TOK_AT` | **Before identifier + `:`:** resource topology resource declaration. **Before identifier + `(`:** resource with protocol. **Before `()` / `{...}`:** anonymous or empty resource. **Before `[`:** retired state-resource prefix, parse error. Source-level bare `@` is not an authoring form. |
| `@stdout` | Standard Output | `TOK_AT` + `NAME("stdout")` | Built-in write-only stream resource (fd 1). Scalar write: `expr -> @stdout`; iterable write: `items >> @stdout`. |
| `@stderr` | Standard Error | `TOK_AT` + `NAME("stderr")` | Built-in write-only stream resource (fd 2, unbuffered). Scalar write: `expr -> @stderr`; iterable write: `items >> @stderr`. |
| `@stdin` | Standard Input | `TOK_AT` + `NAME("stdin")` | Built-in read-only stream resource (fd 0). Iterate via `@stdin >> #(line) => {...}`. Internal declaration forms use `@ stdin := #() => { ... }` with `{ <\|[>_] }`, `{ <\|(>_) }`, and expanded `{ <\| <- [>_] }`. Legacy `(<< @stdin)` is compatibility-only, not canonical design spelling. |
| `$` | Capture / Format Prefix | `TOK_DOLLAR` | **Before `(`:** capture list in function declaration context. **Before string:** format string. **Before identifier:** retired state-resource state reference, parse error. New topology text reads resources through `@name[...]`. |

---

## 2. Data Flow Operators

| Symbol | Name | C++ Token Kind | Semantics | Example |
|--------|------|----------------|-----------|---------|
| `>>` | Pipe / Iterate / Resource-Write Shorthand | `ITERATOR` | **Before iterator tail:** treat the left side as an iterable or pulse source, advance it one item at a time, and push each item as a pulse into the right-side channel/consumer; enter a block-entry snapshot when the consumer is a block. **Before resource atom (`@file(...)`, `@stdout`, `@stderr`, `@stdin`)**: parse as `resource_write` shorthand. **Before `[>_]`, `@stdout`, `@stderr`, or `@file(...)`:** iterable text serialization only, one item per pulse; plain strings must use `->` or explicit `text.lines() >> ...`. `@stdin` remains semantically read-only for data flow. | `prices >> #(p) => { ... }`, `items >> @stdout`, `text.lines() >> [>_]` |
| `\|\|>` | Task Launch / Task Group | `TASK_LAUNCH` | **Before `{`:** construct one scheduled task block and enter a task snapshot context. **Before `[`:** launch independent task blocks and bind each entry name to a task handle; each entry block is its own snapshot/commit stage. | `job = \|\|> { <\| 42 }`, `\|\|> [ t1 := { <\| 1 } ]` |
| `?\|` | Resource Eval / Await / Freeze | `AWAIT_PIPE` | **Before resource operation:** settle the resource effect at that source site; without fallback, failure raises immediately; with `\| fallback`, recovery enters normal type inference; with `\| effect => handler`, only the named typed effect is handled. Handler chains are allowed. `\| ...` is an audited discard only for standalone statements: it settles the resource operation, discards business recovery, produces no value, and continues with the next statement. `\| effect => @()` is rejected because `@()` is a resource sink, not an executable empty action. **With task source:** await or pull a task/future handle into a newly declared typed local through the same effect-evaluation rule. **Without source:** reserved bare continuation freeze; parsed but fail-closed until continuation lowering lands. | `?\| @("log.txt").close()`, `?\| res -> msg_queue \| backpressure => do_something()`, `?\| res -> msg_queue \| ...`, `?\| close_log() \| cleanup_failure => report_cleanup()`, `?\| job -> value: i64`, `?\| job -> value: i64 \| 0`, `?\| -> input: i64` |
| `->` | Forward / Redirect / Resource Sink | `TOK_ARROW_RIGHT` | Redirect data to a physical destination or flow a produced value into a named resource sink | `ma5 -> @database(...)`, `price -> @prices` |
| `<-` | Acquire / Pull | `TOK_ARROW_LEFT` | Extract or acquire from a resource; used in expanded stdin symbolic definition as `<\| <- [>_]`. | `f <- @file("data.txt")` |
| `<<` | Copy / Snapshot / Compatibility Pull | `TOK_SHIFT_BACK` | Explicit resource copy or snapshot, e.g. `snapshot << @price[...]`. Retired history-probe selector families remain rejected. **`(<< @res)`:** compatibility instant pull only. |
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
| `\|` | Value Fallback / Else | `TOK_PIPE_SINGLE` | — | Else-branch for guard conditionals and value-level runtime absence recovery. It is not resource fallback; resource fallback is `?\| operation \| fallback`. |

---

## 4. Guard & Selector Operators

| Syntax | Name | Context | Semantics |
|--------|------|---------|-----------|
| `x[i]` | Index | Postfix on indexable value/resource | Single element read; negative indices count from the end |
| `x[a..b]` | Slice | Postfix on sliceable value/resource | Range slice from `a` to `b`; two or more dots normalize |
| `x[a..]` | Tail Slice | Postfix on sliceable value/resource | Slice from `a` through the end |
| `x[..b]` | Prefix Slice | Postfix on sliceable value/resource | Slice from the start through `b` |
| `x[..]` / `x[...]` | All Selector | Postfix on sliceable value/resource | Select all currently enumerable values |
| `[?, cond]` | Retired Predicate Guard | Inactive old syntax | Use `?(cond) => value \| fallback` or `?(cond) => { ... }` |
| `[?=, val]` | Retired Equality Probe | Inactive old syntax | Use `?=` match blocks |
| Retired history-probe selector | Retired History Probe | Inactive old syntax | Use resource-object selectors such as `@price[-1]` and `@price[-3..]` |
| `[avg, n]` | Moving Average | Postfix on stream/state input | Active compiler intrinsic for the i64 pulse-ledger path; see `Styio-StdLib-Intrinsics.md` for limits |
| `[max, n]` | Rolling Maximum | Postfix on stream/state input | Active compiler intrinsic for the i64 pulse-ledger path; see `Styio-StdLib-Intrinsics.md` for limits |
| `[min, n]`, `[std, n]`, `[ema, n]`, `[rsi, n]` | Deferred series intrinsics | Not active syntax contract | Design candidates only until parser, Sema, lowering, runtime/codegen, and tests land |

`x[a..b]` is a postfix slice selector because it has a left-hand receiver. By
contrast, naked `a..b` is an expression-level range, and `[a..b]` with no
left-hand receiver is a materialized range source rather than a list literal
containing one range expression.

---

## 5. Control Flow Symbols

| Symbol | Name | C++ Token Kind | Semantics |
|--------|------|----------------|-----------|
| `?=` | Pattern Match | `TOK_MATCH` | Trigger pattern matching block; the match body and selected arm block follow block-entry snapshot/commit semantics |
| `?(expr)` | Guard / Paren marker | `TOK_QUEST` + `(` | **As an expression:** `?(expr) => value \| fallback`. **As a statement:** `?(expr) => { ... }` with optional fallback `\| { ... }`; block branches follow block-entry snapshot/commit semantics. **After `[...] >>`:** `?(expr) =>` → conditioned loop (`InfiniteLoopAST`). |
| `=>` | Map / Then | `TOK_FAT_ARROW` | Connects pattern/param to result/body; when the result is a block, it creates a snapshot/commit block stage |
| `^` ... `^^^^` | Break | `BREAK_TOKEN` | Exit the nearest enclosing loop; count is normalized to 1 |
| `>>` ... `>>>>` | Continue | `CONTINUE_TOKEN` | Standalone statement only. Skip the rest of the current block for this pulse/session and resume at the next pulse/session of the nearest continue-capable domain; token length is ignored. |
| `[...]` | Infinite Generator / All Selector | `[` + dot run + `]` | Without a left side, produces an infinite pulse stream. After a value/resource, selects all currently enumerable values. |
| `start..end` | Range Expression | `ELLIPSIS` | Naked expression-level range. It is not a list literal. Step range spelling is reserved and not active syntax. |
| `[start..end]` | Materialized Range | `[` + `ELLIPSIS` + `]` | Materializes `start..end` as an iterable `list[i64]` source. `[start..end] >> #(x) => { ... }` pushes each element into the consumer one at a time. |
| `&` | Stream Zip | `TOK_AMPERSAND` | Align two streams (both must deliver) |

---

## 6. Assignment & Binding

| Symbol | Name | C++ Token Kind | Semantics |
|--------|------|----------------|-----------|
| `=` | Mutable Binding | `TOK_ASSIGN` | Bind or rebind a mutable name. Under `#`, rebinds a callable or operation-channel endpoint. |
| `:=` | Final Binding | `TOK_BIND` | Bind a final name. Under `#`, binds a final callable or operation-channel endpoint that cannot be redefined. |
| `+=` | Aggregate Assign | `TOK_PLUS_ASSIGN` | Accumulate (semi-ring fold in stream context) |
| `-=` | Subtract Assign | `TOK_MINUS_ASSIGN` | Subtract-accumulate |
| `*=` | Multiply Assign | `TOK_STAR_ASSIGN` | Multiply-accumulate |
| `/=` | Divide Assign | `TOK_SLASH_ASSIGN` | Divide-accumulate |

---

## 7. Type & Definition

| Symbol | Name | Semantics |
|--------|------|-----------|
| `#` | Callable / Operation-Channel Binding Prefix | Marks the binding target as callable or operable and combines with `=` or `:=`. It is not a resource prefix; resource identities stay in the `@` family. |
| `:` | Type Annotation | Binds a type to identifier (`a: i32`, `# f : f32 = ...`) |
| `[]` | Type Argument List | In type position, applies type arguments: `list[i64]`, `dict[string, string]` |
| `__ : T := U` | Type Rewrite Rule | Two or more underscores define a type-pattern rewrite, e.g. `__ : list[T] := T..` |
| `T\|n\|` | Exact Length Type | Sequence/cardinality type with exactly `n` values of `T` |
| `T\|..n\|` | Recent Length Type | Recent-window type that keeps the latest `n` values of `T` |
| `T..` / `T...` | Infinite Repetition Type | Unbounded repetition of `T`; two or more dots are equivalent |
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
| `@` alone | Retired source-level undefined value |
| `@ident : Type` | resource topology resource declaration |
| `@ident(...)` | Resource with explicit protocol |
| `@ident{...}` | Invalid explicit-resource spelling |
| `@{...}` or `@(...)` | Anonymous resource (auto-detect) |
| `@stdout`, `@stderr`, `@stdin` | Standard stream resource atom; direct user use is backed by internal Styio prelude declarations |
| `$` followed by identifier | Retired state-resource state reference family; parse error |
| `$(...)` | Capture list (function context) |
| `$"..."` | Format string |
| `>>` after expr, before `#`/`{`/ident | Pipe operator |
| `>>`, `>>>`, `>>>>`, ... as standalone statement | Continue; token length is ignored |
| Retired history-probe selector family inside brackets | Use `@name[-1]`, `@name[-3..]`, or `@name[...]` |
| `list[T]` in type position | Type argument list |
| `x[i]` / `x[a..b]` after indexable value | Index or slice selector |
| `start..end` without a left-hand receiver | Range expression |
| `[start..end]` without a left-hand receiver | Materialized range source, not a single-element list literal |
| `[start..end..step]` | Reserved step range spelling; rejected by active syntax |
| `T..` / `T...` in type position | Infinite repetition type suffix |
| `T|n|` / `T|..n|` in type position | Exact-length or recent-window type suffix |
| `(<- @res)` in parens | Immediate pull |
| `(<< @res)` in parens | Legacy compatibility pull |
| `<~` | Reserved token (always 2-char token); parser rejects active use |
| `~>` | Reserved token (always 2-char token); parser rejects active use |
| `^` contiguous | Break (count ignored; always nearest loop) |
| `^^ ^^` with space | **Illegal** — two separate breaks, rejected by parser |

---

## Appendix: Implementation Notes

### Symbol Density Mitigation

Styio has many symbolic constructs, so symbol docs group them by leading-character family:

- **`>` family:** `>`, `>>`, `>>>`, `>=`, `>_`, `~>` (reserved)
- **`<` family:** `<`, `<<`, `<=`, `<-`, `<|`, `<~` (reserved), `<:`
- **`|` family:** `|`, `||`, `|]`, `|<|`, `|;`
- **`@` family:** resource declarations, resource atoms, anonymous resources, and retired state-family prefixes
- **`$` family:** capture lists, format strings, and retired state-reference prefixes
- **`?` family:** `?`, `?=`, `?(...)`, `??`

The lexer should process these families using a **trie-based dispatch** after reading the first character. This avoids the combinatorial explosion of a flat switch-case.

### Recommended C++ Token Enum Extension

The existing `StyioOpType` enum should be extended with:

```cpp
TOK_WAVE_LEFT,       // <~ reserved
TOK_WAVE_RIGHT,      // ~> reserved
TOK_DOLLAR_PAREN,    // $(
TOK_DOLLAR_STRING,   // $"..."
TOK_DBQUESTION,      // ??
TOK_AMPERSAND,       // & (stream zip)
TOK_BREAK,           // ^...^ normalized to nearest-loop break
TOK_CONTINUE,        // >>...> standalone continue; count ignored
```
