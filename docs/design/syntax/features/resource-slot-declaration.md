# Resource Slot Declaration

**Purpose:** Own top-level resource slot declarations, shaped resource types, and resource-block attachment.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.slot-declaration"
title = "Resource Slot Declaration"
kind = "resource-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Resource Topology"
syntax = "@name : T|..n|"
resolution = "Use `@name` for visible resource identity and type-position shapes for bounded resource storage."
golden_cases = ["tests/features/state_resources/t03_window_avg.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md", "docs/design/syntax/ACTIVE-SYNTAX.md"]
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
symbol = "parse_resource_decl_after_at_latest"
owner = "Parser / Resource Topology"

[dependencies]
requires = [
  { id = "core.type-expression", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.type-expression"]
conflicts = []
supersedes = []
after = []
```

## Decision

`@name : TypeShape` declares a resource slot at top level. Resource identity,
capacity, topology, and lifecycle are explicit; retired state-resource
container spellings are not aliases.

## Evolution Boundary

New resource shapes or declaration scopes must define allocation, topology
ownership, initialization, and destruction before extending this feature.
