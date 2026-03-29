# Styio — Resource Topology & `@` Semantics (Design Spec v2)

**Status:** Target language design — **not fully implemented** in the current compiler.  
**Supersedes (narratively):** informal “parentheses around state” style `@[n](name = …)` as the *preferred* mental model for new code; the **running codebase** still uses M6-era syntax until a migration milestone.  
**See also:** [`Styio-EBNF.md`](./Styio-EBNF.md) (Appendix: Topology v2), [`Logic-Conflicts.md`](./Logic-Conflicts.md).

**文档作用：** `@` 三角色、有界缓冲、`:=` 驱动、影子写入 **`->`** 等 **目标资源拓扑** 的单一叙述；与当前编译器差异见 [`Logic-Conflicts.md`](./Logic-Conflicts.md)。Golden Cross **设计级**示例见 §8；**agent 宪法内嵌代码**仍归 [`AGENT-SPEC.md`](./AGENT-SPEC.md) §12.3。

---

## 1. Why this document exists

A design review concluded that **global, persistent state** must not *look* like a **local function call** (parentheses suggest “instant / local”). This document fixes:

- **Visual semantics:** state is a **storage qualifier** and/or a **top-level resource** bound to a **driver engine**, not a nested “macro call”.
- **Data flow:** writes into shadow containers use **`->` (sink)**, not **`=`**, to enforce **single source of truth** and distinguish **pulse flow** from **mutable binding**.
- **Scope:** declarations of **`@name` resources** belong at **program root** when they are **globally visible**; inner blocks must not silently allocate global ledger slots.

---

## 2. The three roles of `@`

`@` is overloaded but shares one idea: **anything that is not the current tick’s lone pulse** — absence, external drivers, or remembered state — is **anchored** with `@`.

| Role | Meaning | Typical surface form |
|------|---------|------------------------|
| **A. Honest missing** | Algebraic absence (`@` propagates through ops; wave arms can “do nothing”) | lone `@`, `expr \| @`, `(cond) ~> … \| @` |
| **B. Resource anchor** | External driver / file / exchange handle | `@file{…}`, `@binance{…}`, `<< @resource` |
| **C. State container** | Persistent memory (ring buffer, accumulator, snapshot slot) | `@name : type`, optional `:= { driver }` |

**Lexer/parser rule:** distinguish `@` + not-`[` + not-ident → **undefined**; `@` + `[` → state or type; `@` + ident + `{`/`(` → resource per existing rules.

---

## 3. Type shapes for collections and streams

| Notation | Meaning |
|----------|---------|
| `[...]` | Infinite stream / generator (no fixed array size) |
| `[lo .. hi]` | Finite range / literal expansion (e.g. `[0 .. 2]` → `0,1,2`) |
| `[|n|]` | **Bounded** ring buffer of capacity `n` (physical walls) |
| Scalar types | `f64`, `i64`, `bool`, `string` as today |

**Capacity vs strategy:** A strategy that only needs **current vs previous** tick for a *derived* scalar (e.g. golden cross on **MA values**) may use **`[|2|]`** for the **exported** `@ma5` / `@ma20` slots. The **`p[avg, 20]`** intrinsic still needs **20** samples of **raw `p`**; that memory is **compiler-managed** (implicit ledger slots — see §7), not necessarily the same as the **user-visible** `@ma20` buffer size.

---

## 4. Resource declaration & driver binding (top-level)

**Preferred form:**

```text
@name : Type [ , @name2 : Type2 … ] := {
    StreamTopology
}
```

- **`@name : [|n|]`** — declare a **named global resource** with a **bounded** buffer type.
- **`:= { … }`** — bind **one shared driver** (the stream graph) that **feeds** those resources.
- **Inside the driver:** data enters shadow containers with **`expr -> $name`**, not **`$name = expr`** (see §5).

**Forbidden:**

- Declaring **`@name : …`** inside a **local** block **without** a migration path — compiler error: *global resource cannot be initialized in a local block* (exact wording TBD).

**Multi-resource, single engine:**

```text
@ma5 : [|2|], @ma20 : [|2|] := {
  @file{"tests/m6/data/prices.txt"} >> #(p) => {
    get_ma(p, 5)  -> $ma5
    get_ma(p, 20) -> $ma20
    …
  }
}
```

---

## 5. `->` vs `=` for shadow state

| Form | Meaning |
|------|---------|
| **`expr -> $x`** | **Flow** pulse result into the shadow sink `$x` (resource-backed). |
| **`x = expr`** | Ordinary **local** mutable binding (no `@` / `$` on LHS). |

**Error (semantic):** **`$x = expr`** when `$x` denotes a **resource shadow** created by fixed topology — use **`->`** instead: *cannot overwrite a resource created by fixed assignment; use `->` to pipe into the sink.*

This matches **M5** redirect **`-> @file`** as the same **sink** metaphor for **internal** state buffers.

---

## 6. EBNF core (Topology v2 — summary)

Program shape (informative; full detail in [`Styio-EBNF.md`](./Styio-EBNF.md) Appendix):

```ebnf
Program       ::= { TopLevelDecl } EOF
TopLevelDecl  ::= ResourceDecl | … /* func, import, etc. */

ResourceDecl  ::= "@" Identifier ":" Type
                  { "," "@" Identifier ":" Type }
                  [ ":=" DriverBlock ]

DriverBlock   ::= "{" StreamTopology "}"
StreamTopology::= Expression ">>" "#(" Identifier ")" "=>" Block
                  /* plus other stream forms as today */

Type          ::= ScalarType | BoundedBuffer | …
BoundedBuffer ::= "[|" Integer "|]"
```

**Statements inside blocks:** add **`StateWrite ::= Expression "->" "$" Identifier`** as the **only** sanctioned write into **`$`-backed** global shadows in strict mode; **`Assignment`** remains **`Identifier "=" Expression`** for locals.

---

## 7. Intrinsics and hidden state (`p[avg, n]`)

User writes **`p[avg, 20]`** (or `get_ma(p, 20)`); the compiler:

1. **Fingerprint**s the triple **(source, avg, 20)** for deduplication.
2. **Allocates implicit ledger** slots (e.g. ring of 20 raw values + running sum) — **not** the same as the user’s **`@ma20 : [|2|]`** unless the language ties them explicitly.
3. **Inlines** O(1) update + **returns** a scalar **per tick**.

Thus **visible** `@ma20 : [|2|]` may store **only the last two published MA values** for **cross detection**, while **hidden** buffers hold the **20** raw ticks required by the intrinsic.

---

## 8. Golden Cross example (design-level)

**Risk in a naive rewrite:** using **`$ma5 = get_ma(…)`** violates §5 — must be **`get_ma(p, 5) -> $ma5`**.

**Directionally correct sketch:**

```text
@ma5 : [|2|], @ma20 : [|2|] := {
  @file{"tests/m6/data/prices.txt"} >> #(p) => {
    # get_ma := (src, n) => src[avg, n]
    get_ma(p, 5)  -> $ma5
    get_ma(p, 20) -> $ma20
    is_golden = ($ma5 > $ma20) && ($ma5[<<, 1] <= $ma20[<<, 1])
    (is_golden) ~> order_logic(p) | @
  }
}
```

**Export / pull** from other streams uses existing **`<<`** instant-pull / snapshot mechanisms relative to **named top-level resources** (exact syntax for *node.resource* may evolve).

---

## 9. Implementation status (this repository)

| Item | Status |
|------|--------|
| `@[n](var = …)` / pulse ledger / `$` / `[avg,n]` | **Implemented** (M6 path) |
| `@name : [|n|]` prefix syntax | **Not implemented** — parse as future work |
| Top-level-only `ResourceDecl` + `:= { driver }` | **Not implemented** |
| Ban `$x =` for shadows in favor of `->` only | **Not implemented** — semantic rule TBD |
| Implicit intrinsic buffers + fingerprint | **Partially** (ledger + intrinsics exist; naming may differ) |

**Next step for compiler work:** 执行清单、分阶段任务、全量修改点与风险登记见 **[`Resource-Topology-v2-Implementation-Plan.md`](./Resource-Topology-v2-Implementation-Plan.md)**（含开发过程 **history** 记录要求）。概要：milestone **M8** / **Topology v2** 分支 — lexer tokens `[|`, `|]`, multi-decl `,` before `:=`, semantic passes for root-only resources, 与 `tests/m6` 的迁移或双轨策略。

---

## 10. References

- Internal: [`Logic-Conflicts.md`](./Logic-Conflicts.md) §1.3 `@` roles  
- Milestones: [`milestones/2026-03-29/M6-StateAndStreams.md`](./milestones/2026-03-29/M6-StateAndStreams.md)  
- History: [`history/2026-03-29.md`](./history/2026-03-29.md)
