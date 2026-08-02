# Resource Zip Iterator

**Purpose:** Own aligned multi-source iteration, parameter binding, finite termination, and duplicate-driver rejection.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.zip-iterator"
title = "Resource Zip Iterator"
kind = "stream-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Resource Topology"
syntax = "lhs >> #(a) & rhs >> #(b) => body"
resolution = "Use `&` between iterator sources as an aligned zip barrier that terminates with the shorter finite input."
golden_cases = ["tests/features/stream_processing/t01_zip_collections.styio", "tests/pipeline_cases/p06_zip_files/input.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Resource-Topology.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/NewParserExpr.cpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/stream_processing/t01_zip_collections.styio", "tests/pipeline_cases/p06_zip_files/input.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/stream_processing/t01_zip_collections.styio"
stream-driver-contract = "docs/rollups/IM-D5-STREAM-CONCURRENCY-INVENTORY.md"

[implementation]
path = "src/StyioParser/NewParserExpr.cpp"
symbol = "parse_iterator_tail_nightly_draft"
owner = "Parser / Resource Topology"

[dependencies]
requires = [
  { id = "core.list-iterator", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.list-iterator"]
conflicts = []
supersedes = []
after = []
```

## Decision

Zip advances sources in aligned frames and stops at the shorter finite input.
Materialized lists, files, stdin/file pairs, and bounded selector snapshots must
meet their own driver contracts. Duplicate `@stdin & @stdin` remains rejected.

## Evolution Boundary

Teeing, buffering, longest-input behavior, pressure scheduling, or duplicate
external drivers require a stream-driver decision before this feature expands.
