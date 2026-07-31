# Fixed Inference Defaults

**Purpose:** Own the durable rule that scalar inference defaults are fixed by the language and empty collections require local concrete context rather than project, module, backend, or import configuration.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.fixed-inference-defaults"
title = "Fixed Inference Defaults"
kind = "type-defaulting-policy"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / Type System"
syntax = "No default declaration or project/module configuration form; explicit scalar and collection type annotations remain the only author override."
resolution = "Keep `i64` and `f64` as the fixed implicit scalar defaults, run numeric-only relation defaulting once after constraint solving, and require expression-local concrete list/dict context for empty collections."
golden_cases = ["tests/features/literal_defaulting/t01_numeric_literal_normalization.styio", "tests/features/literal_defaulting/t02_contextual_empty_collections.styio", "tests/features/literal_defaulting/e01_empty_list_without_context.styio", "tests/features/literal_defaulting/e02_empty_dict_without_context.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/TypeInfer.cpp"]
evidence = ["tests/features/literal_defaulting/t01_numeric_literal_normalization.styio", "tests/features/literal_defaulting/t02_contextual_empty_collections.styio", "tests/features/literal_defaulting/e01_empty_list_without_context.styio", "tests/features/literal_defaulting/e02_empty_dict_without_context.styio"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/literal_defaulting/t01_numeric_literal_normalization.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
defaulting-contract = "docs/design/syntax/features/core-ambiguous-literal-defaulting.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "solve_callable_constraint_instance"
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.ambiguous-literal-defaulting", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.ambiguous-literal-defaulting"]
conflicts = []
supersedes = []
after = []
```

## Decision

Q9-A was approved on 2026-07-31 and is already implemented. Unannotated
integer and floating literals normalize to `i64` and `f64`. After equality and
callable constraints reach a fixed point, a remaining relation variable
defaults to `i64` only when every fact mentioning it is numeric.

Empty `[]` and `dict {}` never invent element, key, or value types. A concrete
typed binding, established same-expression mutable target, callable argument,
or callable result may provide local context. Later statements and unrelated
imports cannot.

## Diagnostic and Compatibility Boundary

Underconstrained numeric relations request a concrete surrounding annotation.
Empty collections identify the literal and required list/dictionary context.
Neither diagnostic suggests project configuration or backend selection.

Projects and modules cannot redefine defaults. Existing explicit `i8`–`i128`,
`f32`/`f64`, list, and dictionary annotations remain valid and authoritative.

## Evolution Boundary

A future representation-polymorphic literal may be proposed only with an
explicit surrounding type or closed compiler constraint. Import order, project
configuration, target backend, overload priority, and first-observed use may
never become defaulting inputs.
