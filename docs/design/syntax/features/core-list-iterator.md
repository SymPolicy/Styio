# List Iterator

**Purpose:** Own ordinary iterable pulse transfer into `#(item) => body`, including source, binding, and statement boundaries.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.list-iterator"
title = "List Iterator"
kind = "stream-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "[items] >> #(item) => body"
resolution = "Use `>>` to advance an iterable one item at a time and push each item into a pulse closure."
golden_cases = ["tests/features/control_flow/t05_for_each.styio", "tests/pipeline_cases/p05_snapshot_accum/input.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp"]
evidence = ["tests/features/control_flow/t05_for_each.styio", "tests/pipeline_cases/p05_snapshot_accum/input.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/control_flow/t05_for_each.styio"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "parse_list_expr_or_iterator_nightly_draft"
owner = "Parser / Grammar"

[dependencies]
requires = [
  { id = "core.callable-binding", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.expression-precedence", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.callable-binding"]
conflicts = []
supersedes = []
after = []
```

## Decision

An iterator advances the source and binds each produced item to the pulse
closure parameter. `>>` in this position is distinct from a standalone
variable-length continue statement.

## Evolution Boundary

New iterable sources must define finiteness, ownership, item type, failure
behavior, and statement-boundary consumption before joining this feature.
