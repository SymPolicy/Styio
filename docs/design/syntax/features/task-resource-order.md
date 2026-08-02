# Task Resource Order

**Purpose:** Own explicit task happens-before edges without implying data transfer.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "task.resource-order"
title = "Task Resource Order"
kind = "task-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Task Runtime"
syntax = "t1 => t2"
resolution = "Use `=>` between task handles as an explicit happens-before edge, separate from value or resource transfer."
golden_cases = ["tests/features/task_resources/t02_task_flow_right.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/task_resources/t02_task_flow_right.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/task_resources/t02_task_flow_right.styio"
task-runtime-contract = "docs/rollups/IM-D5-STREAM-CONCURRENCY-INVENTORY.md"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "ResourceOrderAST::Create"
owner = "Parser / Task Runtime"

[dependencies]
requires = [
  { id = "task.single-task", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["task.single-task"]
conflicts = []
supersedes = []
after = []
```

## Decision

`t1 => t2` constrains task execution order. It carries no business value and no
implicit resource payload; data movement uses the owning resource or binding
feature.

## Evolution Boundary

Fan-in, fan-out, cycles, conditional ordering, or scheduling priorities require
explicit graph-validation and deadlock decisions.
