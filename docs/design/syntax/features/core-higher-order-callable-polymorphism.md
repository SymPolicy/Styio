# Higher-Order Callable Polymorphism

**Purpose:** Decide whether a generalized callable may remain polymorphic when passed, stored, captured, or returned as a first-class value.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.higher-order-callable-polymorphism"
title = "Higher-Order Callable Polymorphism"
kind = "callable-value-semantics"
decision_state = "review"
delivery_state = "not_started"
owner = "Sema / IR"
syntax = "No new source form selected; existing callable/value positions require an explicit semantic boundary."
resolution = "Owner review pending: keep first-class callable values monomorphic or introduce higher-rank checking and a typed closure representation."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]

[prerequisites]
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"
ir-contract = "docs/rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md"

[implementation]

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.effect-aware-callable-generalization", decision_state = "review", delivery_state = "not_started" },
  { id = "core.constrained-callable-relations", decision_state = "review", delivery_state = "not_started" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = ["core.effect-aware-callable-generalization", "core.constrained-callable-relations"]
```

## Decision Needed

Decide whether a named generalized callable keeps its scheme when it becomes a
value, or whether value position instantiates and freezes one monomorphic
function type. Also decide whether captured local callables may generalize.

## Recommendation

Keep the current accepted feature limited to direct named calls. A callable
that is passed, stored, captured, or returned first instantiates to one concrete
monomorphic function type. Reopen generalized values only after typed callable
IR, closure environment ownership, effect summaries, and a deliberate
bidirectional higher-rank checking rule exist.

## Diagnostic Boundary

Diagnostics should identify the value-position escape and the concrete
annotation needed; they must not suggest authored `forall` or callable type
arguments.

## Evolution Boundary

Rank-2 callbacks, impredicative containers, polymorphic fields, and generalized
capturing closures require separate child decisions.
