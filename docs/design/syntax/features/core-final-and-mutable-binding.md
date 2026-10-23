# Final And Mutable Binding

**Purpose:** Own ordinary Styio value binding syntax, finality, typed binding composition, and rebinding boundaries.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.final-and-mutable-binding"
title = "Final And Mutable Binding"
kind = "core-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "name := expr, name = expr, name: T := expr"
resolution = "Use `:=` for final bindings and `=` for mutable bindings, with optional type annotations resolved in type position."
golden_cases = ["tests/features/scalar_expressions/t07_typed_bind.styio", "tests/features/final_bindings/t01_bounded_final_bind.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp"]
evidence = ["tests/features/scalar_expressions/t07_typed_bind.styio", "tests/features/final_bindings/t01_bounded_final_bind.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/final_bindings/t01_bounded_final_bind.styio"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "parse_stmt_subset_impl_nightly"
owner = "Parser / Grammar"

[dependencies]
requires = [
  { id = "core.expression-precedence", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.type-expression", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = []
conflicts = []
supersedes = []
after = []
```

## Decision

`:=` establishes a final binding and `=` establishes or rebinds a mutable
binding. Type annotations constrain the binding but do not introduce authored
type variables.

## Evolution Boundary

New binding categories must extend this model explicitly. They may not assign a
second meaning to `=` or `:=`, and they must state whether finality applies to
identity, contents, or both.
