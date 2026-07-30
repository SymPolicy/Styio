# Single Task

**Purpose:** Own single-task launch syntax, task-handle binding, capture boundary, and task runtime evidence.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "task.single-task"
title = "Single Task"
kind = "task-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Task Runtime"
syntax = "job = ||> { ... }"
resolution = "Launch one task with `||>` and bind its explicit task handle through the ordinary mutable or final binding model."
golden_cases = ["tests/features/task_resources/t01_task_pull_left.styio", "tests/features/task_resources/t04_task_capture_i64.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/task_resources/t01_task_pull_left.styio", "tests/features/task_resources/t04_task_capture_i64.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/task_resources/t01_task_pull_left.styio"
task-runtime-contract = "docs/rollups/IM-D5-STREAM-CONCURRENCY-INVENTORY.md"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "TaskBlockAST::Create"
owner = "Parser / Task Runtime"

[dependencies]
requires = [
  { id = "core.callable-binding", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.final-and-mutable-binding", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.final-and-mutable-binding"]
conflicts = []
supersedes = []
after = []
```

## Decision

`||> { ... }` launches one task and produces a task handle. Captures and
resource access are explicit semantic obligations; task launch does not create
implicit data transfer.

## Evolution Boundary

Cancellation, priorities, detached tasks, or new capture families must define
ownership and happens-before behavior before extending this feature.
