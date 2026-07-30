# Type Expression

**Purpose:** Own Styio type-expression syntax, bounded and unbounded shapes, collection rewrites, and tuple-type composition.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.type-expression"
title = "Type Expression"
kind = "type-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "T|n|, T|..n|, T.., T..., list[T], dict[K, V], and tuple types"
resolution = "Keep one type-position grammar for exact, recent-window, unbounded, collection-rewrite, and tuple shapes."
golden_cases = ["tests/features/stdio_input/t08_stdin_typed_f64_list_pull.styio", "tests/features/final_bindings/t02_bounded_read.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/features/stdio_input/t08_stdin_typed_f64_list_pull.styio", "tests/features/final_bindings/t02_bounded_read.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/final_bindings/t02_bounded_read.styio"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_styio_type"
owner = "Parser / Grammar"

[dependencies]
requires = []
requires_any = []
extends = []
conflicts = []
supersedes = []
after = []
```

## Decision

Type expressions are resolved only in type position. `T|n|` and `T|..n|`
describe exact and recent-window resource shapes; collection names are
documented rewrites rather than an independent generic language.

## Evolution Boundary

A new shape constructor must define its delimiter ownership, nesting behavior,
runtime representation, and interaction with resource topology before this
feature can advance.
