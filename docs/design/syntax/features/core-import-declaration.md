# Import Declaration

**Purpose:** Own symbol-headed module import syntax and the boundary between grammar recognition and module resolution.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.import-declaration"
title = "Import Declaration"
kind = "module-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "@import { pkg/module }"
resolution = "Open module import grammar with `@` and inspect `NAME(\"import\")` only inside that symbol-headed family."
golden_cases = ["tests/features/scalar_expressions/t20_combined.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/features/scalar_expressions/t20_combined.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/scalar_expressions/t20_combined.styio"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_import_decl_after_at_latest"
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

Imports are opened by `@`; `import` remains a `NAME` outside that grammar
family. Parsing establishes the module path shape, while package discovery and
resolution remain separate contracts.

## Evolution Boundary

Aliases, selective imports, relative paths, or version constraints must extend
this feature without introducing a globally reserved word.
