# Ambiguous Literal Defaulting

**Purpose:** Decide whether and when unresolved numeric literals and empty collection elements receive default types after callable-relation inference.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.ambiguous-literal-defaulting"
title = "Ambiguous Literal Defaulting"
kind = "type-defaulting"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Sema / Type System"
syntax = "Ordinary numeric and collection literals; no callable type-argument syntax."
resolution = "Never default empty collections; normalize numeric literals to the canonical scalar-width contract and default unresolved numeric-only variables once in a final expression-local phase."
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
backend-type-contract = "docs/specs/AGENT-SPEC.md"

[implementation]

[dependencies]
requires = [
  { id = "core.context-driven-call-instantiation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.constrained-callable-relations", decision_state = "accepted", delivery_state = "not_started" },
]
requires_any = []
extends = ["core.context-driven-call-instantiation"]
conflicts = []
supersedes = []
after = ["core.constrained-callable-relations"]
```

## Decision

Empty collection element variables never default and require concrete
surrounding context. Numeric literals normalize to Styio's canonical scalar
widths, `i64` and `f64`. A still-unresolved variable carrying only numeric
constraints defaults once, after equality and constraint solving, within the
smallest enclosing expression. Defaulting never runs during unification and
never uses later statements or module-wide back-propagation.

## Diagnostic Boundary

The diagnostic must distinguish missing context from an unsatisfied operator
constraint and identify the smallest expression that can be annotated.

## Evolution Boundary

User-configurable defaults, module-level default declarations, and
representation-polymorphic defaults require separate decisions.
