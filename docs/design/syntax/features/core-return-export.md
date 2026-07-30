# Return And Export

**Purpose:** Own callable result transfer through `<|` and inline-return forms, including scope and value requirements.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.return-export"
title = "Return And Export"
kind = "control-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "<| expr and inline return value forms"
resolution = "Use symbolic continuation transfer for callable results without introducing a word-headed return statement."
golden_cases = ["tests/features/functions/t06_multi_stmt.styio", "tests/features/control_flow/t10_fizzbuzz.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/syntax/CONTINUATION_TRANSFER.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/CONTINUATION_TRANSFER.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp"]
evidence = ["tests/features/functions/t06_multi_stmt.styio", "tests/features/control_flow/t10_fizzbuzz.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/syntax/CONTINUATION_TRANSFER.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/CONTINUATION_TRANSFER.md"
golden-evidence = "tests/features/functions/t06_multi_stmt.styio"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "parse_return_nightly"
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

`<| expr` transfers a value from the current callable continuation. Inline
return syntax remains a scoped transfer form and may not be reinterpreted as a
general resource write or stream operator.

## Evolution Boundary

New transfer destinations, early-exit categories, or value-less returns must
define their owning continuation and interaction with resource settlement
before extending this feature.
