# Resource Sink Write

**Purpose:** Own scalar and whole-value sink writes into explicit resource identities.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.sink-write"
title = "Resource Sink Write"
kind = "resource-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Resource Topology"
syntax = "expr -> @resource"
resolution = "Use `->` for one value or whole-value serialization into a resource sink."
golden_cases = ["tests/features/state_resources/t03_window_avg.styio", "tests/features/stdio_output/t01_stdout_string.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/features/state_resources/t03_window_avg.styio", "tests/features/stdio_output/t01_stdout_string.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Resource-Topology.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/RESOURCE_IDENTIFIERS.md"
golden-evidence = "tests/features/stdio_output/t01_stdout_string.styio"
resource-topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "try_parse_resource_write_tail_latest"
owner = "Parser / Resource Topology"

[dependencies]
requires = [
  { id = "core.expression-precedence", decision_state = "accepted", delivery_state = "converged" },
  { id = "resource.slot-declaration", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["resource.slot-declaration"]
conflicts = []
supersedes = []
after = []
```

## Decision

`expr -> @resource` emits one pending write to the visible resource sink.
Commit, serialization, mutability, and sink capability remain governed by the
resource family.

## Evolution Boundary

Batching, transactional grouping, or multi-writer behavior must define commit
order and conflict handling rather than overloading the scalar write silently.
