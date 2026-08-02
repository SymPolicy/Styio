# Infinite Loop

**Purpose:** Own the infinite iterable marker and its conditional pulse-loop composition.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.infinite-loop"
title = "Infinite Loop"
kind = "control-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "[...] >> ?(cond) => { ... } and infinite body forms"
resolution = "Represent an infinite pulse source with `[...]` and compose termination through the ordinary guard family."
golden_cases = ["tests/features/control_flow/t06_inf_break.styio", "tests/features/control_flow/t07_while.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp"]
evidence = ["tests/features/control_flow/t06_inf_break.styio", "tests/features/control_flow/t07_while.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/control_flow/t06_inf_break.styio"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "parse_infinite_conditional_loop_after_iterator_nightly_draft"
owner = "Parser / Grammar"

[dependencies]
requires = [
  { id = "core.conditional-expression", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.list-iterator", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.list-iterator", "core.conditional-expression"]
conflicts = []
supersedes = []
after = []
```

## Decision

`[...]` is an infinite pulse source, not an ordinary collection value. It must
feed an iterator/guard body whose control transfers define continuation and
termination.

## Evolution Boundary

Scheduling, cancellation, fairness, or backpressure changes must state whether
they alter this source contract or only its runtime driver.
