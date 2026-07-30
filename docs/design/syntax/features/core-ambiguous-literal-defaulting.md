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
delivery_state = "converged"
owner = "Sema / Type System"
syntax = "Ordinary numeric and collection literals; no callable type-argument syntax."
resolution = "Never default empty collections; normalize numeric literals to the canonical scalar-width contract and default unresolved numeric-only variables once in a final expression-local phase."
golden_cases = ["tests/features/literal_defaulting/t01_numeric_literal_normalization.styio", "tests/features/literal_defaulting/t02_contextual_empty_collections.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]
implementation = ["src/StyioSema/TypeInfer.cpp", "src/StyioSema/SemaContext.hpp"]
evidence = ["tests/features/literal_defaulting/t01_numeric_literal_normalization.styio", "tests/features/literal_defaulting/t02_contextual_empty_collections.styio", "tests/features/literal_defaulting/e01_empty_list_without_context.styio", "tests/features/literal_defaulting/e02_empty_dict_without_context.styio", "tests/features/literal_defaulting/e03_numeric_literal_fixes_scalar_relation.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/design/Styio-EBNF.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/literal_defaulting/t01_numeric_literal_normalization.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"
backend-type-contract = "docs/specs/AGENT-SPEC.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "solve_callable_constraint_instance"
owner = "Sema / Type System"

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

Sema now normalizes unannotated integer and floating literals to `i64` and
`f64` before constructing callable relations. A relation fixed by a numeric
literal does not silently change between integer and floating families at a
later call. A relation variable left unresolved after equality and constraint
solving defaults to `i64` only when every remaining fact about that variable is
`numeric`.

An empty list or `dict {}` carries no fabricated element/value type. It is
accepted only when a concrete surrounding list or dictionary type reaches the
literal inside the same expression, including a typed binding, callable result,
or callable argument context. Without that context, Sema reports the empty
literal itself as underconstrained. Defaulting never consults a later statement
or mutates a relation during unification.

The `literal_defaulting` goldens prove canonical numeric normalization,
contextual empty list/dictionary acceptance, missing-context diagnostics for
both empty collection families, and rejection of a floating instance for a
relation fixed by an integer literal.

## Evolution Boundary

User-configurable defaults, module-level default declarations, and
representation-polymorphic defaults require separate decisions.
