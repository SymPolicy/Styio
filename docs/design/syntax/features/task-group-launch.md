# Task Group Launch

**Purpose:** Own grouped task launch syntax, entry bindings, and group-level scheduling boundaries.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "task.group-launch"
title = "Task Group Launch"
kind = "task-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Task Runtime"
syntax = "||> [ t1 := { ... } t2 := { ... } ]"
resolution = "Launch a statically named group of tasks through one `||>` group expression with explicit task-handle bindings."
golden_cases = ["tests/features/task_resources/t06_task_group_await.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/task_resources/t06_task_group_await.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/task_resources/t06_task_group_await.styio"
task-runtime-contract = "docs/rollups/IM-D5-STREAM-CONCURRENCY-INVENTORY.md"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "parse_task_group_launch_nightly"
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

A group launch creates multiple named task handles under one structural launch
form. Grouping does not imply data dependencies or completion order; those
remain explicit.

## Evolution Boundary

Dynamic group membership, structured cancellation, result aggregation, or
resource quotas require separate scheduling and ownership decisions.
