# Callable Binding

**Purpose:** Own callable and operation-channel binding syntax, finality, inference ownership, and callable-body boundaries.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.callable-binding"
title = "Callable Binding"
kind = "callable-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "# name = #(arg: T) => expr, # name := (arg: T) => expr, and block-bodied callable forms"
resolution = "Use `#` to mark callable identity while reusing `=` and `:=` for mutable and final binding."
golden_cases = ["tests/features/functions/t01_simple_func.styio", "tests/pipeline_cases/p02_simple_func/input.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/HashFunctionParser.hpp"]
evidence = ["tests/features/functions/t01_simple_func.styio", "tests/pipeline_cases/p02_simple_func/input.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/functions/t01_simple_func.styio"

[implementation]
path = "src/StyioParser/HashFunctionParser.hpp"
symbol = "parse_hash_function_common_latest"
owner = "Parser / Grammar"

[dependencies]
requires = [
  { id = "core.expression-precedence", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.final-and-mutable-binding", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.type-expression", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.final-and-mutable-binding"]
conflicts = []
supersedes = []
after = []
```

## Decision

Callable identities stay visibly in the `#` family and inherit ordinary binding
finality. Generic relations are inferred at definitions and uses; authored
generic binders and call-site specialization are outside this feature.

## Evolution Boundary

Closure capture, recursion, overloads, or callable interface publication must
extend this document and its dependency graph rather than adding another
callable declaration head.
