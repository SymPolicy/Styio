# Resource Selector Read

**Purpose:** Own resource latest, slice, and materialized snapshot read syntax and result-shape boundaries.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.selector-read"
title = "Resource Selector Read"
kind = "resource-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Resource Topology"
syntax = "@name[-1], @name[-3..], @name[...]"
resolution = "Read committed or block-snapshot resource state through explicit resource-object selectors."
golden_cases = ["tests/features/state_resources/t03_window_avg.styio", "tests/features/state_resources/t05_frame_lock.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/features/state_resources/t03_window_avg.styio", "tests/features/state_resources/t05_frame_lock.styio"]

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
symbol = "parse_resource_ref_after_at_latest"
owner = "Parser / Resource Topology"

[dependencies]
requires = [
  { id = "resource.selector-offset", decision_state = "accepted", delivery_state = "converged" },
  { id = "resource.slot-declaration", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["resource.selector-offset"]
conflicts = []
supersedes = []
after = []
```

## Decision

Latest selectors return one committed value; bounded slices and full bounded
snapshots materialize values according to the resource shape. Unsupported
unbounded snapshots remain fail-closed.

## Evolution Boundary

Tuple history, unbounded snapshots, or additional resource families require
separate capability and lifetime decisions before this read surface expands.
