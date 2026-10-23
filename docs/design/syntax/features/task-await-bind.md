# Await Bind

**Purpose:** Own task settlement, typed result binding, fallback behavior, and await diagnostics.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "task.await-bind"
title = "Await Bind"
kind = "task-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Task Runtime"
syntax = "?| job -> value: T | fallback"
resolution = "Settle a task through the shared effect marker, bind its typed result, and keep fallback behavior explicit."
golden_cases = ["tests/features/task_resources/t01_task_pull_left.styio", "tests/features/task_resources/t06_task_group_await.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/task_resources/t01_task_pull_left.styio", "tests/features/task_resources/t06_task_group_await.styio"]

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
symbol = "parse_await_bind_stmt_nightly"
owner = "Parser / Task Runtime"

[dependencies]
requires = [
  { id = "core.type-expression", decision_state = "accepted", delivery_state = "converged" },
  { id = "task.single-task", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["task.single-task"]
conflicts = []
supersedes = []
after = []
```

## Decision

Await uses the typed effect-settlement family. Success binds the task result;
failure follows the explicit fallback or raises through the effect contract.

## Evolution Boundary

Timeouts, cancellation results, multi-await, or partial group settlement must
define result typing and failure ordering before extending this feature.
