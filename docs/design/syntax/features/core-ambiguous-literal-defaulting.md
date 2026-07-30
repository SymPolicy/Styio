# Ambiguous Literal Defaulting

**Purpose:** Decide whether and when unresolved numeric literals and empty collection elements receive default types after callable-relation inference.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.ambiguous-literal-defaulting"
title = "Ambiguous Literal Defaulting"
kind = "type-defaulting"
decision_state = "review"
delivery_state = "not_started"
owner = "Sema / Type System"
syntax = "Ordinary numeric and collection literals; no callable type-argument syntax."
resolution = "Owner review pending: decide which ambiguous literal variables may default, at what phase, and to which canonical scalar types."
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
  { id = "core.constrained-callable-relations", decision_state = "review", delivery_state = "not_started" },
]
requires_any = []
extends = ["core.context-driven-call-instantiation"]
conflicts = []
supersedes = []
after = ["core.constrained-callable-relations"]
```

## Decision Needed

Decide independently whether:

1. an unconstrained integer or floating literal defaults;
2. an empty list or dictionary element variable defaults; and
3. defaulting occurs per expression or after a wider binding/module pass.

## Recommendation

Never default empty collection elements; require a concrete surrounding
annotation. Resolve the current source/backend scalar-width contract before
choosing numeric defaults, then run numeric defaulting once as a visible final
expression-local inference phase. Until then, reject ambiguous constrained
calls rather than guessing.

## Diagnostic Boundary

The diagnostic must distinguish missing context from an unsatisfied operator
constraint and identify the smallest expression that can be annotated.

## Evolution Boundary

User-configurable defaults, module-level default declarations, and
representation-polymorphic defaults require separate decisions.
