# Match Cases

**Purpose:** Own Styio pattern-match case syntax, fallback requirements, expression boundaries, and match evidence.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.match-cases"
title = "Match Cases"
kind = "control-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "expr ?= { pattern => { ... } _ => { ... } }"
resolution = "Use `?=` for expression-oriented matching with explicit case arrows and `_` fallback."
golden_cases = ["tests/features/control_flow/t01_match.styio", "tests/features/control_flow/t02_match_expr.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp"]
evidence = ["tests/features/control_flow/t01_match.styio", "tests/features/control_flow/t02_match_expr.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/control_flow/t01_match.styio"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "try_parse_hash_let_match_nightly_latest"
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

Match operates on an expression and routes through ordered pattern cases. `_`
is the explicit fallback spelling; case result compatibility is a semantic
decision rather than parser recovery.

## Evolution Boundary

New pattern families must define exhaustiveness, binding scope, result-type
joining, and diagnostics without weakening the existing fallback contract.
