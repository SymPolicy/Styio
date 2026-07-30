# Effect-Aware Callable Generalization

**Purpose:** Decide when a callable that performs or captures effects may be generalized and how future effect information participates in its inferred scheme.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.effect-aware-callable-generalization"
title = "Effect-Aware Callable Generalization"
kind = "callable-type-semantics"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Sema / Type System"
syntax = "No new source syntax proposed; this decision governs eligibility of final callable bindings for generalization."
resolution = "Generalize only closed, proven-pure final callable bodies; unknown or effectful bodies remain monomorphic, while inferred effect rows are reserved for a later feature."
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

## Decision

Styio adopts a two-stage rule. A call-graph member is generalizable only when
its free environment is closed and every reachable operation is proven pure.
Mutable captures, resource operations, task operations, fallback/handler
operations, unclassified native calls, and unknown callee summaries fail
closed to the existing monomorphic callable path.

Compiler-owned inferred effect rows remain an explicit later extension. They
must not be simulated by syntactic exceptions or implicit trust annotations.

## Prerequisite Boundary

Delivery requires a stable effect summary, propagation across callable SCCs,
capture analysis, and a diagnostic taxonomy that distinguishes
non-generalizable effects from ordinary type conflicts.

## Evolution Boundary

This feature does not decide handler syntax, effect-row source notation, native
purity annotations, or resource capability polymorphism.
