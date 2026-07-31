# Rank-2 Callback Polymorphism

**Purpose:** Preserve the approved long-term boundary for context-checked generalized callback parameters without enabling inferred impredicative callable values.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.rank2-callback-polymorphism"
title = "Rank-2 Callback Polymorphism"
kind = "higher-rank-callable-semantics"
decision_state = "deferred"
delivery_state = "not_started"
owner = "Sema / Type System"
syntax = "No source `forall` or generic binder is selected; a future higher-order parameter may carry one compiler-owned generalized callback requirement."
resolution = "Reserve bidirectional, context-checked rank-2 callback parameters as the only approved higher-rank direction; do not infer impredicative lists, dictionaries, fields, captures, or arbitrary generalized values."
reopen_when = "Reopen only after core.monomorphic-callable-values, core.affine-capturing-closures, core.canonical-effect-rows, and core.capability-usage-polymorphism are all accepted/converged with typed callable IR evidence."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/TypeInfer.cpp"]
evidence = ["tests/features/README.md"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/README.md"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
typed-ir-boundary = "docs/rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md"

[implementation]
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.monomorphic-callable-values", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.affine-capturing-closures", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.canonical-effect-rows", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.capability-usage-polymorphism", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.higher-order-callable-polymorphism"]
conflicts = []
supersedes = []
after = ["core.affine-capturing-closures"]
```

## Decision

Q3-A was approved as a deferred direction on 2026-07-31. When reopened, the
first higher-rank surface is a callback parameter whose required generalized
relation is supplied by its enclosing callable contract and checked
bidirectionally. A named function may satisfy that requirement without turning
the function into an impredicative runtime value.

There is no source `forall`, no inferred higher-rank result, and no
generalized callable stored in a list, dictionary, field, capture, or ordinary
binding.

## Reopen and Diagnostic Boundary

All four dependencies named in `reopen_when` must converge first. Reopening
also requires evidence that effect and capability facts survive callable-value
lowering and interface publication.

A future checker must distinguish “monomorphic callback type mismatch” from
“callback does not satisfy the required generalized relation.” It must never
repair an ambiguous expression by guessing a rank or by generalizing a mutable
or capturing value.

## Compatibility and Evolution Boundary

The current direct-call rank-1 behavior and Q1 monomorphic values remain
unchanged. Fully impredicative values, polymorphic fields/containers, inferred
higher-rank arguments, and source generic binders remain unapproved.
