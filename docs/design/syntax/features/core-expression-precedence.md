# Expression Precedence

**Purpose:** Own the Styio expression-precedence feature, including its canonical operator ordering, parser authority, dependencies, and convergence evidence.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.expression-precedence"
title = "Expression Precedence"
kind = "core-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "arithmetic, comparison, boolean, call, index, member, and parenthesized expressions"
resolution = "Keep one compiler-owned expression hierarchy with call, index, and member postfix forms binding above infix operators."
golden_cases = ["tests/features/scalar_expressions/t20_combined.styio", "tests/pipeline_cases/p01_print_add/input.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp"]
evidence = ["tests/features/scalar_expressions/t20_combined.styio", "tests/pipeline_cases/p01_print_add/input.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/scalar_expressions/t20_combined.styio"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "parse_expr_subset_nightly"
owner = "Parser / Grammar"

[dependencies]
requires = []
requires_any = []
extends = []
conflicts = []
supersedes = []
after = []
```

## Decision

Styio accepts one precedence hierarchy for scalar operators, postfix calls,
selectors, members, and parentheses. The nightly compiler parser is the
acceptance authority; editor parsers may represent the same structure but do
not decide validity.

## Evolution Boundary

Adding an operator, changing associativity, or moving an operator between
precedence levels changes this feature SSOT and requires downstream
revalidation of every feature that consumes expressions.
