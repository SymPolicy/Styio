# Materialized Range

**Purpose:** Own expression ranges, bracketed range materialization, iterator composition, and the reserved step-range boundary.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.materialized-range"
title = "Materialized Range"
kind = "collection-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "[start..end] and [start..end] >> #(item) => body; [start..end..step] is reserved"
resolution = "Keep naked `start..end` as the range expression and brackets as explicit materialization into a list source."
golden_cases = ["tests/features/control_flow/t08_multi_break.styio", "tests/features/control_flow/t11_recursive_board_assignment_eight_queens.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/features/control_flow/t08_multi_break.styio", "tests/features/control_flow/t11_recursive_board_assignment_eight_queens.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/control_flow/t08_multi_break.styio"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_list_exprs_latest_draft"
owner = "Parser / Grammar"

[dependencies]
requires = [
  { id = "core.expression-precedence", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.list-iterator"]
conflicts = []
supersedes = []
after = []
```

## Decision

`start..end` is an expression-level range. `[start..end]` materializes that
range as `list[i64]`; it is not a one-element list containing a range value.
Step ranges remain reserved and parser-rejected.

## Evolution Boundary

Activating step ranges requires a new decision covering direction, zero steps,
overflow, endpoint inclusion, materialization cost, and iterator semantics.
