# Resource Instant Pull

**Purpose:** Own immediate value extraction from a resource atom and its failure boundary.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.instant-pull"
title = "Resource Instant Pull"
kind = "resource-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Resource Topology"
syntax = "<< @resource"
resolution = "Use prefix `<<` to request an immediate value from a readable resource without opening an iterator."
golden_cases = ["tests/features/stream_processing/t04_instant_pull.styio", "tests/pipeline_cases/p07_instant_pull/input.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/features/stream_processing/t04_instant_pull.styio", "tests/pipeline_cases/p07_instant_pull/input.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Resource-Topology.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/RESOURCE_IDENTIFIERS.md"
golden-evidence = "tests/features/stream_processing/t04_instant_pull.styio"
resource-topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_instant_pull_resource_atom_latest"
owner = "Parser / Resource Topology"

[dependencies]
requires = [
  { id = "resource.slot-declaration", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["resource.slot-declaration"]
conflicts = []
supersedes = []
after = []
```

## Decision

Prefix `<<` performs one immediate readable-resource pull. Availability and I/O
failures remain typed resource effects; the syntax does not imply an infinite
or repeated read.

## Evolution Boundary

Timeouts, asynchronous pulls, or multi-value reads must define settlement and
result typing before extending this expression family.
