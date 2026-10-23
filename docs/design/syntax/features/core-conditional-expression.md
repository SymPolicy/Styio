# Conditional Expression

**Purpose:** Own inline and block guard syntax, boolean guard semantics, fallback shape, and conditional-expression evidence.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.conditional-expression"
title = "Conditional Expression"
kind = "control-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "?(cond) => expr | fallback and block-bodied conditionals"
resolution = "Use the symbol-headed `?()` guard family for value and block conditionals."
golden_cases = ["tests/features/wave_dispatch/t01_wave_merge.styio", "tests/features/wave_dispatch/t03_wave_dispatch.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp"]
evidence = ["tests/features/wave_dispatch/t01_wave_merge.styio", "tests/features/wave_dispatch/t03_wave_dispatch.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/wave_dispatch/t01_wave_merge.styio"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "parse_guard_value_expr"
owner = "Parser / Grammar"

[dependencies]
requires = [
  { id = "core.expression-precedence", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = []
conflicts = []
supersedes = []
after = []
```

## Decision

`?(cond)` opens the conditional family. Guards must type as boolean; inline
branches must join to a valid result type, while block branches retain statement
semantics.

## Evolution Boundary

Additional guard chaining or pattern guards must preserve the symbol-headed
family and state whether they extend conditional evaluation or pattern
matching.
