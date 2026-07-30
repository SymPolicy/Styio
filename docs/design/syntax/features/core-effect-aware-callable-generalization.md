# Effect-Aware Callable Generalization

**Purpose:** Decide when a callable that performs or captures effects may be generalized and how future effect information participates in its inferred scheme.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.effect-aware-callable-generalization"
title = "Effect-Aware Callable Generalization"
kind = "callable-type-semantics"
decision_state = "review"
delivery_state = "not_started"
owner = "Sema / Type System"
syntax = "No new source syntax proposed; this decision governs eligibility of final callable bindings for generalization."
resolution = "Owner review pending: choose a purity/value restriction now and the boundary for a future inferred effect-row extension."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]

[prerequisites]
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
effect-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"

[implementation]

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = []
```

## Decision Needed

Choose whether generalization is:

1. restricted to closed, proven-pure final callable bodies;
2. based on a relaxed syntactic value restriction; or
3. immediately extended with inferred effect rows.

The decision must classify mutable captures, resource acquire/read/write/close,
task launch/await, fallback handlers, native calls, and calls whose effects are
not yet summarized.

## Recommendation

Adopt option 1 now and preserve option 3 as an explicit extension. A call graph
member is generalizable only when its free environment is closed and every
reachable operation is proven pure. Unknown effect facts fail closed. Later,
compiler-owned effect rows can make safe effect polymorphism more expressive
without adding authored generic syntax.

## Prerequisite Boundary

Delivery requires a stable effect summary, propagation across callable SCCs,
capture analysis, and a diagnostic taxonomy that distinguishes
non-generalizable effects from ordinary type conflicts.

## Evolution Boundary

This feature does not decide handler syntax, effect-row source notation, native
purity annotations, or resource capability polymorphism.
