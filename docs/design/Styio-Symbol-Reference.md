# Styio Symbol Reference

**Purpose:** 各符号的 **lexer token 名与物理含义速查表**；完整语义与章节论证见 [`Styio-Language-Design.md`](./Styio-Language-Design.md)。实现 `enum class TokenKind` 时以本文与 EBNF 对照。

**Last updated:** 2026-07-20

**Version:** 1.0-draft  
**Date:** 2026-03-28

This document serves as the definitive lookup table for all symbols in Styio. It is the primary reference for implementing `enum class TokenKind` in the C++ lexer.

---

## 1. Resource & State Identifiers

| Symbol | Name | C++ Token Kind | Physical Semantics |
|--------|------|----------------|-------------------|
| `@` | Resource Anchor | `TOK_AT` | **Before identifier + `:`:** resource topology resource declaration. **Before identifier + `(`:** resource with protocol. **Before `()` / `{...}`:** anonymous or empty resource. **Before `[`:** retired state-resource prefix, parse error. Bare `@` is a parse error and never denotes value absence; Optional empty is `(?)`, `[?]`, or `{?}` under `? \| T`. |
| `@stdout` | Standard Output | `TOK_AT` + `NAME("stdout")` | Built-in write-only stream resource (fd 1). Scalar write: `expr -> @stdout`; iterable write: `items >> @stdout`. |
| `@stderr` | Standard Error | `TOK_AT` + `NAME("stderr")` | Built-in write-only stream resource (fd 2, unbuffered). Scalar write: `expr -> @stderr`; iterable write: `items >> @stderr`. |
| `@stdin` | Standard Input | `TOK_AT` + `NAME("stdin")` | Built-in read-only stream resource (fd 0). Iterate via `@stdin >> #(line) => {...}`. Internal declaration forms use `@ stdin := #() => { ... }` with `{ <\|[>_] }`, `{ <\|(>_) }`, and expanded `{ <\| <- [>_] }`. Legacy `(<< @stdin)` is compatibility-only, not canonical design spelling. |
| `$` | Capture / Format Prefix | `TOK_DOLLAR` | **Before `(`:** capture list in function declaration context. **Before string:** format string. **Before identifier:** retired state-resource state reference, parse error. New topology text reads resources through `@name[...]`. |

---

## 2. Data Flow Operators

| Symbol | Name | C++ Token Kind | Semantics | Example |
|--------|------|----------------|-----------|---------|
| `>>` | Pipe / Iterate / Resource-Write Shorthand | `ITERATOR` | **Before iterator tail:** treat the left side as an iterable or pulse source, advance it one item at a time, and push each item as a pulse into the right-side channel/consumer; enter a block-entry snapshot when the consumer is a block. **Before resource atom (`@file(...)`, `@stdout`, `@stderr`, `@stdin`)**: parse as `resource_write` shorthand. **Before `[>_]`, `@stdout`, `@stderr`, or `@file(...)`:** iterable text serialization only, one item per pulse; plain strings must use `->` or explicit `text.lines() >> ...`. `@stdin` remains semantically read-only for data flow. Multi-role service (pipe / resource write / continue) is a settled design requirement; disambiguation is compiler-owned context logic and the roles will not be split across symbols. | `prices >> #(p) => { ... }`, `items >> @stdout`, `text.lines() >> [>_]` |
| `\|\|>` | Task Launch / Task Group | `TASK_LAUNCH` | **Before `{`:** construct one scheduled task block and enter a task snapshot context. **Before `[`:** launch independent task blocks and bind each entry name to a task handle; each entry block is its own snapshot/commit stage. | `job = \|\|> { <\| 42 }`, `\|\|> [ t1 := { <\| 1 } ]` |
| `?\|` | Operation Settlement / Callable Completion Bound | `AWAIT_PIPE` *(legacy enum name)* | **Expression start:** settle exactly one complete operation. Success bypasses recovery; one matching arm runs lazily once; no implicit retry; unhandled families propagate. **After a callable result annotation:** `?\| {io, parse}` declares a non-empty finite completion-family upper bound. Its comma list contains ordinary family identifiers and is not a runtime set/dict, union, fallback, or handler. `: T` without this clause always declares the empty bound; `?\| {}`, duplicates, non-family names, and trailing commas are rejected. The contiguous token is distinct from spaced Optional `? \| T`. | `answer = ?\| job \| fallback`, `# read : f64 ?\| {io, parse} := (path: string) => { ... }` |
| `\|?\|` | Resource Session Marker | `TOK_SESSION_MARK` | Explicit resource session block; no `session` keyword. Mid-transfer placement only (execution symbols before and after). Body: handles and anchors only; topology `@name : Type` stays rejected. Design-accepted; parser-pending. | `# f => \|?\| { h <- @file("log.txt") } \|> g` |
| `\|!\|` | Session Exit Special Handling | `TOK_SESSION_EXIT` | `|!|(cleanup)` / `|!|(ResourceCleanupFailure)` marks session-exit special handling for the cleanup effect family. Absent `|!|` and absent deferred cleanup chain → default Close. Not a universal exception catch-all. Design-accepted; parser-pending. | `... \|?\| { ... } \|!\|(cleanup) => report()` |
| `\|>` | Settlement Forward | `TOK_SETTLE_FWD` | After a `|?|` session leaves, forward settlement or control to the next stage. Activated for this role; `|<-` remains reserved. Design-accepted; parser-pending. | `# f := \|?\| { ... } \|> g` |
| `->` | Directional Transfer | `TOK_ARROW_RIGHT` | Flow the value produced at the left location/action into the destination/location/receiver endpoint drawn on the right. This describes data direction, not an affine ownership move. The right side never declares a name and must independently resolve as a writable endpoint. Successful transfer produces `() : unit`, never an implicit source, destination, or receipt. Source value and endpoint capability are independent prerequisites; arrow direction does not impose source-before-endpoint preparation. Two order-sensitive preparations must first be sequenced as Block items. Endpoint kinds may determine capabilities, completion families, ownership action, and lowering without changing the arrow's meaning. | `operation -> result`, `job -> answer`, `ma5 -> @database(...)` |
| `<-` | Acquire / Pull | `TOK_ARROW_LEFT` | Extract or acquire from a resource; used in expanded stdin symbolic definition as `<\| <- [>_]`. | `f <- @file("data.txt")` |
| `<<` | Copy / Snapshot / Compatibility Pull | `TOK_SHIFT_BACK` | Explicit resource copy or snapshot, e.g. `snapshot << @price[...]`. Retired history-probe selector families remain rejected. **`(<< @res)`:** compatibility instant pull only. Deliberately low-priority surface by settled decision: type-directed unification of `<<` is deferred with no active convergence work scheduled. |
| `<\|` | Lexical Block Yield | `YIELD_PIPE` | **Statement start:** complete only the immediately owning lexical Block with the expression value. It never crosses a parent Block; only the outer function-body Block result becomes a function result. The infix apply-pipe spelling is removed. | `<\| x * x` |
| `\|<\|` | Inline Lexical Block Yield | `RETURN_PIPE` | Same node and current-Block target as `<\|`; the exact one-line form must end with `\|;`. | `...; \|<\| result \|;` |
| `\|;` | Statement Separator / Inline-Yield Terminator | `PIPE_SEMICOLON` | General explicit separator for compressed one-line Blocks; mandatory terminator inside the `\|<\| expr \|;` form and semantically inert there. | `x = 1; \|<\| x \|;` |
| `>_` | Terminal Device | `TOK_IO_BUF` | **As statement:** `>_(expr)` prints to stdout (legacy). **As value:** first-class terminal device handle. Canonical symbolic spelling is `[>_]`; `(>_)` remains compatibility. | `>_("hello")`, `<\|(>_)` |

`->` is deliberately read as a picture of data movement, not as a token whose
meaning is selected from assignment/export/redirection/task-binding modes. The
endpoint types may cause very different code generation, but every accepted
edge preserves the same left-to-right **data direction**. This phrase never
means left-to-right preparation time. `?|` is orthogonal and settles the whole
operation; it never changes the arrow's meaning. The accepted completion
contract fixes Unit success and an independently valid destination; Q03-F in
[Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md)
fixes the dependency/order boundary. Chaining, associativity, ownership, and
backpressure scheduling remain with their focused owners.

---

## 3. Reserved Waves and Anchored Single Pipe

| Symbol | Name | C++ Token Kind | Direction | Semantics |
|--------|------|----------------|-----------|-----------|
| `<~` | Reserved Wave Left | `TOK_WAVE_LEFT` | Reserved | Reserved symbol only: participates in no syntax feature until the language design explicitly declares an active semantics; parser rejects every use |
| `~>` | Reserved Wave Right | `TOK_WAVE_RIGHT` | Reserved | Reserved symbol only: participates in no syntax feature until the language design explicitly declares an active semantics; parser rejects every use |
| `\|` | Anchored Separator | `TOK_PIPE_SINGLE` | Contextual | Consumed only by an already-open grammar production: type union such as `? \| T`, guard else after `?(cond) => ...`, or fallback/named-family separation after leading `?\|`. It is never a general binary value operator. `a \| b`, `true \| false`, `0 \| 1`, bare-pipe chains, and the retired settlement discard `?\| op \| ...` are parser errors (Language Design §7.4). |

---

## 4. Guard & Selector Operators

| Syntax | Name | Context | Semantics |
|--------|------|---------|-----------|
| `x[i]` | Index | Postfix on indexable value/resource | Single element read; negative indices count from the end |
| `x[a..b]` | Slice | Postfix on sliceable value/resource | Range slice from `a` to `b`; two or more dots normalize |
| `x[a..]` | Tail Slice | Postfix on sliceable value/resource | Slice from `a` through the end |
| `x[..b]` | Prefix Slice | Postfix on sliceable value/resource | Slice from the start through `b` |
| `x[..]` / `x[...]` | All Selector | Postfix on sliceable value/resource | Select all currently enumerable values |
| `[?, cond]` | Retired Predicate Guard | Inactive old syntax | Use `?(cond) => then_value \| else_value` or `?(cond) => { ... }` |
| `[?=, val]` | Retired Equality Probe | Inactive old syntax | Use `?=` match blocks |
| Retired history-probe selector | Retired History Probe | Inactive old syntax | Use resource-object selectors such as `@price[-1]` and `@price[-3..]` |
| `x[%n]` | Stride Selector | Active postfix selector on a sliceable value/resource | Keep every element at index ≡ 0 (mod n), counting from the first selected element. `n` is a positive integer; `[%1]` is identity; literal `[%0]` is rejected statically and dynamic non-positive strides fail through the runtime error channel. No left operand exists inside the bracket, so `[%` never collides with binary modulo. |
| `[avg, n]` / `[max, n]` | Removed Word-Mode Selectors | Removed design spelling | Selectors are a pure-symbol algebra: no identifier participates in selector syntax. Series intrinsics use ordinary call syntax `avg(series, n)` / `max(series, n)` recognized in Sema (matrix-helper model); the parser's bracket path is compatibility debt. See `Styio-StdLib-Intrinsics.md`. |
| `[min, n]`, `[std, n]`, `[ema, n]`, `[rsi, n]` | Removed word-mode spellings for deferred intrinsics | Not syntax | Deferred series intrinsics land as ordinary calls only (`min(series, n)`, `rsi(series, n)`) once evidence exists; no word-mode selector spelling will be added |

`x[a..b]` is a postfix slice selector because it has a left-hand receiver. By
contrast, naked `a..b` is an expression-level range, and `[a..b]` with no
left-hand receiver is a materialized range source rather than a list literal
containing one range expression.

For index and slice selectors, the receiver and every required index/bound are
strict prerequisites. Their written positions do not imply receiver-first or
left-to-right preparation. Two unordered order-sensitive prerequisites are a
Q03-F static error and must be prepared in consecutive Block items.

---

## 5. Control Flow Symbols

| Symbol | Name | C++ Token Kind | Semantics |
|--------|------|----------------|-----------|
| `?=` | Pattern Match | `TOK_MATCH` | Trigger pattern matching block; the match body and selected arm block follow block-entry snapshot/commit semantics |
| `?(expr)` | Guard / Paren marker | `TOK_QUEST` + `(` | **As an expression:** `?(expr) => then_value \| else_value`; both branches are required and join-compatible. **As a statement:** `?(expr) => { ... }` with optional else `\| { ... }`; an omitted else completes with Unit and never synthesizes absence/default. Block branches follow block-entry snapshot/commit semantics. **After `[...] >>`:** `?(expr) =>` → conditioned loop (`InfiniteLoopAST`). |
| `=>` | Map / Then | `TOK_FAT_ARROW` | Connects pattern/parameter to result/body. For a direct one-expression body, `=> expr`, `=> { expr }`, and `=> { <\| expr }` are equivalent; multi-item Blocks have no implicit tail result. A Block body creates its snapshot/commit stage, and its top-level items provide the Q03-F order-sensitive sequence; this does not impose a source order on ordinary sibling operands. |
| `^` ... `^^^^` | Break | `BREAK_TOKEN` | Exit the nearest enclosing loop; count is normalized to 1. Single-level only by settled decision (goto-hell prevention); multi-level break is permanently rejected and not an open question |
| `>>` ... `>>>>` | Continue | `CONTINUE_TOKEN` | Standalone statement only. Skip the rest of the current block for this pulse/session and resume at the next pulse/session of the nearest continue-capable domain; token length is ignored. Single-level only by settled decision; token length never encodes depth |
| `[...]` | Infinite Generator / All Selector | `[` + dot run + `]` | Without a left side, produces an infinite pulse stream. After a value/resource, selects all currently enumerable values. |
| `start..end` | Range Expression | `ELLIPSIS` | Naked expression-level range. It is not a list literal. Step range spellings are removed from the design and rejected. |
| `[start..end]` | Materialized Range | `[` + `ELLIPSIS` + `]` | Materializes `start..end` as an iterable `list[i64]` source. `[start..end] >> #(x) => { ... }` pushes each element into the consumer one at a time. |
| `&` | Stream Zip (Event-Arrival Barrier) | `TOK_AMPERSAND` | The first-arriving value blocks and waits until the other side delivers; the body fires once per matched pair. No staleness, timeout, or tolerance-window policy exists at this operator; the removed `&[expr]` spelling is rejected. |

---

## 6. Assignment & Binding

| Symbol | Name | C++ Token Kind | Semantics |
|--------|------|----------------|-----------|
| `=` | Mutable Binding | `TOK_ASSIGN` | Bind or rebind a mutable name; an RHS expression is mandatory. Under `#`, a bare `# f = ...` only rebinds an already established stable callable scheme. An initial mutable callable requires a complete explicit contract. |
| `:=` | Final Binding | `TOK_BIND` | Bind a final name with a mandatory RHS expression. Under `#`, binds a final callable or operation-channel endpoint that cannot be redefined; only an otherwise eligible final callable value may receive automatic principal rank-1 generalization. |
| `name := $(deps) => expr` | Derived Binding | `TOK_BIND` + `TOK_DOLLAR_PAREN` + `TOK_FAT_ARROW` | Frame-committed derived slot: writes to captured variables mark it dirty; recomputation runs at pulse-frame commit in topological order, at most once per frame; within a frame the value is constant; the value decays to a plain value at use. Governed by the fail-closed whitelist in Language Design §5.3 (module-scope only, pure single-expression body, static graph, no task-block reads). Design-accepted; parser pending; fails closed. |
| `+=` | Aggregate Assign | `TOK_PLUS_ASSIGN` | Accumulate (semi-ring fold in stream context) |
| `-=` | Subtract Assign | `TOK_MINUS_ASSIGN` | Subtract-accumulate |
| `*=` | Multiply Assign | `TOK_STAR_ASSIGN` | Multiply-accumulate |
| `/=` | Divide Assign | `TOK_SLASH_ASSIGN` | Divide-accumulate |

`name : T` is not an ordinary declaration form. `:` may annotate an ordinary
binding only when `=` or `:=` and an explicit RHS follow. A missing RHS never
requests an implicit zero, Unit, absence, uninitialized slot, or default value.
Typed parameters, pattern/iteration binders, schema fields, and resource
topology slots are separate grammatical contexts whose enclosing construct
supplies or governs the value. Settlement results use ordinary binding with an
explicit RHS, for example `answer: T = ?| operation | fallback`; settlement does
not introduce a typed target declaration.

---

## 7. Type & Definition

| Symbol | Name | Semantics |
|--------|------|-----------|
| `#` | Callable / Operation-Channel Binding Prefix | Marks the binding target as callable or operable and combines with `=` or `:=`. It is not a resource prefix; resource identities stay in the `@` family. With `:=`, an eligible capture-safe, non-recursive, non-boundary lexical-local or module-private callable value may infer a stable principal scheme. |
| `:` | Type Annotation | Binds a type to an identifier in its enclosing construct (`a: i32` as a parameter, `x: i32 = 1` as an ordinary binding, `# f : f32 = ...` as a callable binding). In a callable contract, `: T` without `?\| {...}` always asserts an empty completion bound; only an eligible lexical-local or module-private callable omitting the entire contract may infer its operation summary. Required boundaries remain explicit. It does not make bare `name: T` an ordinary declaration. |
| `T ?\| {family, ...}` | Callable Completion Upper Bound | After a callable's normal result type, declares a finite non-empty set of nominal families that may cross the boundary. Braces/commas are static signature structure, not a value-level set, dict, Block, or union. Family names remain identifiers. |
| `[]` | Type Argument List | In type position, applies type arguments: `list[i64]`, `dict[string, string]` |
| `? \| T` | Optional Union Type | The only type-level absence boundary. Repeated absence normalizes (`? \| (? \| T) == ? \| T`); `? \| unit` retains absent and present-`()` states even though Unit has no payload bytes. |
| `unit` | Unit Type Name | Ordinary `NAME("unit")` interpreted contextually in type position. Exactly one value `()`; normal generic argument; zero payload never erases logical count, presence, membership, task state, or completion. |
| `never` | Bottom Type Name | Ordinary `NAME("never")` interpreted contextually in type position. Has no values, literal, or default and denotes only proven non-completion. It joins as `join(T, never) = T` and is never an inference fallback. |
| `()` | Unit Value | The sole value of `unit`. A reachable natural Block fallthrough also produces this value. It is not absence or an empty argument list when parsed in value position. |
| `(?)`, `[?]`, `{?}` | Optional Empty Value | Three exact delimiter variants of the same empty Optional branch. They require an expected or joined payload type; none is a distinct empty kind or a default for ordinary `T`. |
| `__ : T := U` | Type Rewrite Rule | Two or more underscores define a type-pattern rewrite, e.g. `__ : list[T] := T..` |
| `T\|n\|` | Exact Length Type | Sequence/cardinality type with exactly `n` values of `T` |
| `T\|..n\|` | Recent Length Type | Recent-window type that keeps the latest `n` values of `T` |
| `T..` / `T...` | Infinite Repetition Type | Unbounded repetition of `T`; two or more dots are equivalent |
| `_` | Wildcard | Default/catch-all in pattern matching |

Principal inference has no source glyph of its own. Compiler displays such as
`forall T` or `Add(T, IntegerLiteral(5), T, Completion(T))` are diagnostic
metalanguage, not author syntax. The complete rule is owned by
[Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md):
[Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md)
supplies the accepted closed built-in literal/`Add` relation, while `F02` owns
any future author-written generic/constraint or completion-row surface.

---

## 8. Arithmetic & Logic

| Symbol | Name | Precedence |
|--------|------|------------|
| `**` | Power | 704 |
| `*` | Multiply | 703 |
| `/` | Divide | 703 |
| `%` | Modulo | 703 |
| `+` | Closed Built-in Add | 702 |
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

`+` keeps its existing token and precedence. Its accepted scalar relation is
closed over `i8` through `i128`, `f32`, `f64`, and exact literals materializable
to those types. Concrete operands must be the same type; an exact literal may
materialize symmetrically to the other concrete operand; the result is that
type. Checked integer addition admits the payload-free prelude completion
identifier `overflow`, while floating addition has an empty completion bound
and strict IEEE behavior. Exact-literal materialization, late `i64`/`f64`
defaults, the cross-row generic `{overflow}` union, and constant/runtime
equivalence are owned
by [Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md).

The `-` glyph in a signed literal participates in that literal's exact value;
this does not freeze the separately deferred binary subtraction relation.
`IntegerLiteral`, `DecimalLiteral`, `Add`, and `Completion` remain compiler
metalanguage, and `overflow` remains an ordinary identifier rather than a
keyword or token.

Operator precedence builds the expression tree; it is not evaluation order.
Ordinary binary operands are strict prerequisites without an implicit
left-to-right time edge, and unordered order-sensitive siblings are rejected.
`&&` and `||` instead carry their defined short-circuit control edge: the right
operand runs only when selected and at most once.

---

## 9. Removed Spellings & Channel Selection

| Symbol | Name | Semantics |
|--------|------|-----------|
| `??` | Removed Value Spelling | D02 is closed with no ordinary value-level fallback/coalescing operator. `??` has no token, grammar, diagnostic-extraction, or fallback role in the target language; the orphan `TOK_DBQUESTION` implementation path must be deleted and every source use rejected. |
| `!(expr)` | Channel Selector | **Before `-> ( >_ )`:** selects stderr channel (fd 2) instead of stdout (fd 1). In other contexts, `!` remains logical NOT. |

---

## 10. Lexer Disambiguation Quick Reference

| Input | Resolution |
|-------|------------|
| `@` alone | Parse error; it has no value/absence role. Write `(?)` only in a `? \| T` context |
| `@ident : Type` | resource topology resource declaration. Top-level only: local-block declarations — including inside `\|?\|` sessions — are rejected (`The global resource cannot be initialized in a local block`). Resource sessions authorize handles and anchors only (Resource Topology §4.2). Scoped subtopology remains a separate fail-closed reserve; first-class dynamic resources are permanently rejected |
| `@ident(...)` | Resource with explicit protocol |
| `@ident{...}` | Invalid explicit-resource spelling |
| `@{...}` or `@(...)` | Anonymous resource (auto-detect) |
| `@stdout`, `@stderr`, `@stdin` | Standard stream resource atom; direct user use is backed by internal Styio prelude declarations |
| `$` followed by identifier | Retired state-resource state reference family; parse error |
| `$(` starting a derived-binding head | Derived binding `name := $(deps) => expr`; design-accepted, parser pending, fails closed. The removed head spelling `name $(deps) := expr` is not syntax |
| `$"..."` | Format string |
| `? \| T` in type position | Optional union delimiter; this anchored type grammar does not create a value operator |
| `?(cond) => A \| B` | Guard else separator; the leading guard fixes the role syntactically |
| `?\| op \| fallback` | Resource/task effect settlement; leading `?\|` fixes every following handler/fallback separator |
| `a \| b`, `true \| false`, `0 \| 1` | Parse error in general value-expression grammar; no inferred type or purity-based reinterpretation |
| `lhs ?? rhs` | Parse error by settled design; the language has no ordinary value fallback/coalescing operator and `??` has no accepted role |
| `>>` after expr, before `#`/`{`/ident | Pipe operator |
| `>>`, `>>>`, `>>>>`, ... as standalone statement | Continue; token length is ignored |
| Retired history-probe selector family inside brackets | Use `@name[-1]`, `@name[-3..]`, or `@name[...]` |
| `list[T]` in type position | Type argument list |
| `x[i]` / `x[a..b]` after indexable value | Index or slice selector |
| `%` directly after `[` | Active stride selector `x[%n]` (no left operand exists inside the bracket, so it is never binary modulo) |
| `avg` / `max` or any identifier directly inside `[...]` selector | Removed word-mode selector spelling; series intrinsics use ordinary call syntax `avg(series, n)`; parser bracket acceptance is compatibility debt |
| `start..end` without a left-hand receiver | Range expression |
| `[start..end]` without a left-hand receiver | Materialized range source, not a single-element list literal |
| `[start..end..step]` | Removed step range spelling; the parser rejects it |
| `T..` / `T...` in type position | Infinite repetition type suffix |
| `T|n|` / `T|..n|` in type position | Exact-length or recent-window type suffix |
| `(<- @res)` in parens | Immediate pull |
| `(<< @res)` in parens | Legacy compatibility pull |
| `.pressure` after a resource expression, before `>>` | Pressure observer stream. `pressure` is a Sema-recognized member attribute, not a grammar word. Delivery is a single-slot conflated latest-wins level sensor; pulses fire only on hysteresis state transitions; payload is a prelude read-only struct (`pending`/`limit`/`peak`). All current families fail closed with `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED` |
| `<~` | Reserved symbol (always one 2-char token); participates in no syntax feature until explicitly activated by the language design; parser rejects every use |
| `~>` | Reserved symbol (always one 2-char token); participates in no syntax feature until explicitly activated by the language design; parser rejects every use |
| `^` contiguous | Break (count ignored; always nearest loop) |
| `^^ ^^` with space | **Illegal** — two separate breaks, rejected by parser |

---

## Appendix: Implementation Notes

### Symbol Density Mitigation

Styio has many symbolic constructs, so symbol docs group them by leading-character family:

- **`>` family:** `>`, `>>`, `>>>`, `>=`, `>_`, `~>` (reserved)
- **`<` family:** `<`, `<<`, `<=`, `<-`, `<|`, `<~` (reserved), `<:`
- **`|` family:** anchored `|`, `||`, `|]`, `|<|`, `|;`
- **`@` family:** resource declarations, resource atoms, anonymous resources, and retired state-family prefixes
- **`$` family:** capture lists, format strings, and retired state-reference prefixes
- **`?` family:** `?`, `?=`, and `?(...)`; adjacent `??` is a removed spelling, not a compound token

The lexer should process these families using a **trie-based dispatch** after reading the first character. This avoids the combinatorial explosion of a flat switch-case.

### Recommended C++ Token Enum Extension

The existing `StyioOpType` enum should be extended with:

```cpp
TOK_WAVE_LEFT,       // <~ reserved
TOK_WAVE_RIGHT,      // ~> reserved
TOK_DOLLAR_PAREN,    // $(
TOK_DOLLAR_STRING,   // $"..."
TOK_AMPERSAND,       // & (stream zip)
TOK_BREAK,           // ^...^ normalized to nearest-loop break
TOK_CONTINUE,        // >>...> standalone continue; count ignored
```
