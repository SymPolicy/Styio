# File Resource Atom

**Purpose:** Own file-backed resource construction through direct and named file atoms.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.file-atom"
title = "File Resource Atom"
kind = "io-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Runtime IO"
syntax = "@file(\"path\") and @(\"path\")"
resolution = "Construct a statically resolved file resource through the `@` resource family, with `@file(\"path\")` and `@(\"path\")` as accepted forms."
golden_cases = ["tests/features/file_resources/t01_read_file.styio", "tests/pipeline_cases/p04_read_file/input.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp", "src/StyioExtern/ExternLib.cpp"]
evidence = ["tests/features/file_resources/t01_read_file.styio", "tests/pipeline_cases/p04_read_file/input.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Resource-Topology.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/RESOURCE_IDENTIFIERS.md"
golden-evidence = "tests/features/file_resources/t01_read_file.styio"
runtime-contract = "src/StyioExtern/ExternLib.cpp"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_resource_file_atom_latest"
owner = "Parser / Runtime IO"

[dependencies]
requires = [
  { id = "core.expression-precedence", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = []
conflicts = []
supersedes = []
after = []
```

## Decision

File resources remain visibly inside the `@` family. Path resolution,
capabilities, acquisition, settlement, and release are resource contracts, not
ordinary string-call semantics.

## Evolution Boundary

URI schemes, virtual files, encoding parameters, or dynamic capability
selection require explicit resource-family decisions.
