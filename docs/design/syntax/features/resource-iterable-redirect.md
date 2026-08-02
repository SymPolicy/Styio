# Resource Iterable Redirect

**Purpose:** Own per-item iterable transfer into writable resource sinks and distinguish it from whole-value writes.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.iterable-redirect"
title = "Resource Iterable Redirect"
kind = "resource-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Resource Topology"
syntax = "items >> @resource"
resolution = "Use `>> @resource` for per-item pulse emission from an iterable and keep `-> @resource` for one whole value."
golden_cases = ["tests/pipeline_cases/p03_write_file/input.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/pipeline_cases/p03_write_file/input.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Resource-Topology.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/RESOURCE_IDENTIFIERS.md"
golden-evidence = "tests/pipeline_cases/p03_write_file/input.styio"
resource-topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_resource_redirect_tail_latest"
owner = "Parser / Resource Topology"

[dependencies]
requires = [
  { id = "core.list-iterator", decision_state = "accepted", delivery_state = "converged" },
  { id = "resource.sink-write", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.list-iterator", "resource.sink-write"]
conflicts = []
supersedes = []
after = []
```

## Decision

Iterable redirection advances its source and writes each produced item as a
separate sink pulse. Plain strings and whole containers use `->` unless their
items are intentionally iterated.

## Evolution Boundary

Parallel emission, chunking, or sink backpressure must extend iterator and
resource-driver contracts together.
