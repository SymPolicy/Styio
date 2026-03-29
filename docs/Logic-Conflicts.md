# Styio — Logic Conflicts & Open Resolutions

**Version:** 1.0  
**Date:** 2026-03-28  
**Purpose:** Single place to record contradictions between design docs, milestones, the existing compiler, and informal discussions. Resolving these is prerequisite to a coherent implementation.

---

## 1. Symbol Overloading (Same Lexeme, Different Semantics)

### 1.1 `<<` — Four (or Five) Meanings

| Meaning | Context | Design reference |
|---------|---------|-------------------|
| **Write to sink** | `expr << @file{...}` or `expr << handle` | `Styio-Language-Design.md` §7.4 |
| **Snapshot binding** | `@[v] << @resource` | §9.2 |
| **Instant pull** | `(<< @resource)` as primary expression | §9.2 |
| **History probe** | `$var[<<, n]` inside `[` `]` | §8.4, §10.5 |
| **Return / extractor (current code)** | Top-level `<<` → `EXTRACTOR` in lexer | `Token.hpp`, `Parser.cpp` `parse_return` |

**Conflict:** The **running compiler** already tokenizes `<<` as `EXTRACTOR` and uses it for return-style syntax. The design adds **I/O write**, **snapshot declaration**, and **instant pull**, all reusing `<<`. The lexer can still emit one `EXTRACTOR` token, but the **parser** must branch on position:

- After `@[id]` → snapshot declaration (M7)
- After `(` as start of `(<< @r)` → instant pull (M7)
- As first token of statement vs `expr << resource` → disambiguate **return** vs **write**
- Inside `[` after `,` as mode `<<` → history probe (not the same token stream as `EXTRACTOR` for `<<` alone — may be `[` then `<<` then `,`)

**Resolution needed:** Write an explicit **disambiguation table** in `Styio-EBNF.md` for `<<` that includes **legacy `EXTRACTOR` return** vs **new write/snapshot/pull**. Decide whether return migrates to `<|` only or keeps `<<`.

---

### 1.2 `>>` — Three Meanings

| Meaning | Context |
|---------|---------|
| **Pipe / iterate** | `source >> #(x) => { ... }` |
| **Continue** | Standalone `>>`, `>>>`, … (length ≥ 2) |
| **Stride selector (planned)** | `[>>, n]` in `Styio-EBNF.md` / symbol reference |

**Conflict:** `ITERATOR` in the lexer is a **run of `>`** (like `>>`, `>>>`, …). Continue uses the **same character** with “standalone statement” context. Pipe uses `>>` **after** an expression.

**Resolution needed:** Parser lookahead rules in `Styio-EBNF.md` Appendix must be implemented exactly; ambiguous cases (e.g. `>>` at start of line inside a block) need examples.

---

### 1.3 `@` — Three Roles

| Role | Example |
|------|---------|
| **Undefined value** | `@` alone |
| **State / window decl** | `@[5](...)`, `@[total = 0](...)` **(current compiler)** |
| **Resource** | `@file{...}`, `@{"path"}` |

**Conflict:** `@` followed by `[` is **state**, but `@` followed by identifier is **resource**. A lone `@` is **undefined**. The parser must not treat `@` as “start of resource” when it is the **value** `@`.

**Resolution needed:** Formal grammar ordering: `@` + not-`[` + not-ident → `UndefinedAST`; `@` + `[` → state or mistake; `@` + ident + `{`/`(` → resource.

**Target design (v2):** Unify narrative under [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md): **`@name : [|n|]`** as **storage qualifier**, top-level **`ResourceDecl`** with optional **`:= { driver }`**, and **`expr -> $name`** for writes. The **running** compiler has **not** switched; M6 syntax remains canonical until a migration milestone.

---

### 1.4 `&` — Bitwise AND vs Stream Zip

| Meaning | Where |
|---------|--------|
| **Bitwise AND** | `StyioOpType::Bitwise_AND`, `Token.hpp` `TOK_AMP` |
| **Stream zip** | `A >> #(a) & B >> #(b) =>` (M7, language design §9) |

**Conflict:** Same character `&`. In expression position between two integers, `&` is bitwise. In pipeline position between two `>>` clauses, `&` is zip.

**Resolution needed:** Define **syntactic positions** where `&` is zip-only (e.g. only between `) >> #(…)` and another stream source). Forbid or parenthesize bitwise `&` in those positions, or use a different zip token (design choice).

---

## 2. Type System & Operations

### 2.1 String Concatenation with `+`

**Milestone tests** use:

- `n + " " + s` (M7 T7.01 — int + string)
- `"Buy at: " + price` (M6 Golden Cross — string + numeric)
- `"Arb: " + gap` (M7 T7.08)

**Design doc** does not define `+` for `string` with `i32`/`f64`, only arithmetic types.

**Conflict:** Tests assume **overloaded `+`** or implicit `to_string`. Not specified in `Styio-Language-Design.md` or intrinsics.

**Resolution needed:** Either add a **string concatenation** rule and coercion table, or change tests to use `>_("...")` with format strings only.

---

### 2.2 File Lines as Numbers (`sum += line`)

**M5 T5.03** uses `sum += line` where `line` is read from a file (string).

**Conflict:** Adding string to numeric accumulator requires **parse to int** or typed stream of `i32`. Not specified.

**Resolution needed:** Define **driver typing** (e.g. `@file` lines as `str`, explicit `line as i32`) or restrict test to already-parsed numeric stream.

---

### 2.3 Printing `@` (M4 T4.06)

**Test expectation:** `>_(x + 10)` where `x = @` → stdout shows `@`.

**Conflict:** If the result is `@`, is `>_` defined for undefined values? Format? Or should output be empty / diagnostic line?

**Resolution needed:** Specify `>_(@)` behavior (literal text `@`, nothing, or `??`-style message).

---

### 2.4 `==` and `@`

**M4 T4.05:** `>_(x == @)` with `x = @` → `true`.

**Conflict:** Requires **equality** on tagged/optional representation. Must align with “`@` is not NaN” (where `NaN != NaN`).

**Resolution needed:** State explicitly: `@ == @` → `true`; `@ == concrete` → `false`; concrete op `@` → `@` or `false` (pick one and document).

---

## 3. Scoping & Control Flow

### 3.1 State / Loop Body vs After Loop (M6 T6.04)

**Test:** Loop `[1..5] >> #(p) => { @[5](price = p) }` then `>_( $price[<<, 1] )` **outside** the closure.

**Conflict:** `$price` is tied to **stream scope** and **pulse timing**. Reading `$price[<<,1]` **after** the stream ends implies:

- Either outer scope can see **last** stream state (ledger lifetime = whole program), or  
- The test is **invalid** (read must be inside the last pulse).

**Resolution needed:** Define **lifetime of state** declared inside `>>` body: program-wide ledger vs stream-local. Golden Cross keeps state **inside** the same `>>` block — T6.04 should match that or be rewritten.

---

### 3.2 Variables Assigned Only Inside Loop (M6 T6.03, T6.08)

**Tests** use `>_(ma)` or `>_(mx)` **after** `collection >> #(p) => { @[n](ma = ...) }`.

**Conflict:** If `ma` exists only inside the closure, it is **not in scope** at top level after the loop. If it is hoisted to anonymous ledger, **which value** is “final” — last pulse’s `ma`?

**Resolution needed:** Same as §3.1: **hoisting rules** and **visible names after stream** must be specified.

---

### 3.3 `>>` Continue Inside `?=` Arms (M3)

**M3** uses `>>` inside match arms for **continue**.

**Conflict:** Inside a nested loop, `>>` could be parsed as start of a **pipe** if the parser is in expression context.

**Resolution needed:** Continue is **statement-level** only; match arm body must be parsed as **block of statements**, not a single expression, unless grammar wraps clearly.

---

## 4. Milestone vs Milestone

### 4.1 M5 T5.03 Uses M6 Syntax

**T5.03** uses `@[total = 0](sum = $total + line)` but is filed under **M5**.

**Documented as:** stretch goal / depends on M6.

**Conflict:** If an agent implements M5 tests literally, T5.03 **cannot pass** until M6.

**Resolution:** Move T5.03 to **M6** or remove `@[...]` from M5 and use only bindings that M5 can support (e.g. explicit accumulator variable — still needs stream semantics).

---

### 4.2 Factorial in M2 vs M3

**M2 T2.08** (recursive factorial with `?=`) is marked **stretch / deferred to M3**.  
**M3 T3.04** repeats factorial.

**Conflict:** Duplicate specification; risk of two different expected behaviors.

**Resolution:** Keep **one canonical** factorial test (M3 only); M2 references “see M3 T3.04”.

---

## 5. Design Doc Internal Tensions

### 5.1 “No Void” vs Side Effects

**§2.3** says everything is expression-oriented; **§6** uses `~> order_logic(p) | @` where branches are **effects**.

**Conflict:** Strict “no void” is incompatible with **effect-only** branches unless they are typed as a unit / `@` / sentinel.

**Resolution:** Clarify: **expression-oriented** applies to **value-producing** control flow; **effect expressions** return `@` or `()`-like sentinel at type level.

---

### 5.2 `:` for Types vs Ternary-Style (Historical)

Design settled on **`<~` / `~>`** for conditionals; `:` is **type annotation** only.

**Conflict:** Older sketches (`idea.md`) may still suggest `:` for other uses. Ignore `idea.md` for semantics unless reconciled.

---

### 5.3 Default Float Type Table Bug

**Token.hpp** maps `f32` to internal name `"f64"` with 32 bits — likely copy-paste error.

**Conflict:** Type system docs say `f32` is 32-bit; table row is inconsistent.

**Resolution:** Fix `DTypeTable` entry for `f32` in code; note in this doc until fixed.

---

## 6. Compiler Implementation vs Design

### 6.1 `**` Power Operator

**Design / M1** require `**` for power.  
**Tokenizer** may not emit a dedicated `**` token (reported gap).

**Conflict:** Parser/CodeGen cannot implement `**` until lexer does.

---

### 6.2 Top-Level `1 + 2 + 3`

**M1** requires full expression at top level.  
**Current** `parse_stmt_or_expr` only parses a **single** literal for `INTEGER`/`DECIMAL`.

**Conflict:** Documented in M1; listed here as **design–implementation gap**.

---

### 6.3 IR Lowering Stubs

**Many AST nodes** lower to `SGConstInt(0)`; **Print**, **Block**, **Call**, etc. do not reach LLVM meaningfully.

**Conflict:** Milestones assume **end-to-end** behavior; current binary **cannot** satisfy M1+ until lowering is fixed.

---

## 7. Research Claims vs Engineering Cost

### 7.1 Pulse Frame Lock vs Performance

**Research doc** claims O(k) snapshot per pulse.  
**Critique** (Gemini / design notes): at extreme tick rates, k-wide snapshot may hurt.

**Conflict:** Not a logical contradiction but a **performance assumption** to validate with benchmarks.

---

### 7.2 Intent-Aware I/O vs Dynamic Files

**Design** assumes drivers can **fail-fast** on missing columns/paths.  
**Plain text files** have no schema.

**Conflict:** “Compile-time” intent for `.txt` is limited to **conventions** or **runtime** first-read validation.

---

## 8. Recommended Resolution Order

1. **Freeze `<<` / `>>` / `@` / `&` disambiguation** — update `Styio-EBNF.md` + `Styio-Symbol-Reference.md` with the tables from §1 and §2.4.
2. **Decide string `+` and `>_@`** — update language design + fix milestone tests if needed.
3. **Fix scoping tests (T6.03, T6.04, T6.08)** — align with ledger lifetime rules.
4. **Re-home T5.03** — move to M6 or simplify.
5. **Fix `f32` in `DTypeTable`** — code fix.
6. **Deduplicate factorial tests** — M2/M3.

---

## 9. How to Use This Document

- **Before changing grammar:** Check §1–§3 for clashes.
- **Before adding a test:** Ensure types and scoping are defined (§2–§3).
- **Before claiming a milestone done:** Resolve or explicitly **defer** every conflict that touches that milestone’s tests.

When a conflict is **resolved**, add a dated note under that subsection or remove it and point to the canonical spec section.
