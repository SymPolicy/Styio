# Standard Input Pull

**Purpose:** Own scalar, tuple, and typed-list binding from the standard-input resource.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.stdin-pull"
title = "Standard Input Pull"
kind = "io-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Runtime IO"
syntax = "value <- @stdin and typed tuple/list stdin pull"
resolution = "Bind one standard-input read through `<- @stdin`, using an explicit target type when scalar inference is insufficient."
golden_cases = ["tests/features/stdio_input/t02_stdin_pull.styio", "tests/features/stdio_input/t07_stdin_typed_tuple_pull.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Resource-Topology.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp", "src/StyioExtern/ExternLib.cpp"]
evidence = ["tests/features/stdio_input/t02_stdin_pull.styio", "tests/features/stdio_input/t07_stdin_typed_tuple_pull.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Resource-Topology.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/RESOURCE_IDENTIFIERS.md"
golden-evidence = "tests/features/stdio_input/t02_stdin_pull.styio"
runtime-contract = "src/StyioExtern/ExternLib.cpp"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "try_parse_typed_stdin_pull_bind_latest"
owner = "Parser / Runtime IO"

[dependencies]
requires = [
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

`<- @stdin` performs one input operation and binds the result. Tuple and list
targets use their explicit type contract to parse multiple values; duplicate
consumption remains a driver-level decision.

## Evolution Boundary

Interactive prompts, byte streams, decoding modes, or duplicated input drivers
must define ownership and buffering before changing this feature.
