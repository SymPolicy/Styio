# Resource Selector Offset

**Purpose:** Own resource selector offset and range parsing shared by latest, bounded-history, and snapshot reads.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.selector-offset"
title = "Resource Selector Offset"
kind = "resource-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Resource Topology"
syntax = "resource selector offsets and ranges"
resolution = "Parse selector offsets as a bounded resource-specific subgrammar without changing ordinary index expression precedence."
golden_cases = ["tests/features/state_resources/t03_window_avg.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/features/state_resources/t03_window_avg.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Resource-Topology.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/RESOURCE_IDENTIFIERS.md"
golden-evidence = "tests/features/state_resources/t03_window_avg.styio"
resource-topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_resource_selector_offset_latest"
owner = "Parser / Resource Topology"

[dependencies]
requires = [
  { id = "core.expression-precedence", decision_state = "accepted", delivery_state = "converged" },
  { id = "resource.slot-declaration", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = []
conflicts = []
supersedes = []
after = []
```

## Decision

Resource selector offsets and ranges are interpreted against committed resource
history and the current block snapshot. Their bounds are not interchangeable
with arbitrary collection indexing when topology guarantees differ.

## Evolution Boundary

New selector modes must define static bounds, unavailable-history behavior,
result shape, and whether they materialize an iterable snapshot.
