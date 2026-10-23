# Resource Method Definition

**Purpose:** Own resource-family property and method bindings, receiver scope, finality, and value-returning body boundaries.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "resource.method-definition"
title = "Resource Method Definition"
kind = "resource-syntax"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Resource Topology"
syntax = "@family::name := () => { ... }"
resolution = "Bind resource-family methods and properties through `@family::name`, reusing ordinary binding finality while keeping receiver identity explicit."
golden_cases = ["tests/features/stream_processing/t07_singleton.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/RESOURCE_IDENTIFIERS.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/stream_processing/t07_singleton.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Resource-Topology.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/RESOURCE_IDENTIFIERS.md"
golden-evidence = "tests/features/stream_processing/t07_singleton.styio"
resource-topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_resource_method_def_after_at_latest"
owner = "Parser / Resource Topology"

[dependencies]
requires = [
  { id = "core.callable-binding", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.return-export", decision_state = "accepted", delivery_state = "converged" },
  { id = "resource.slot-declaration", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.callable-binding", "resource.slot-declaration"]
conflicts = []
supersedes = []
after = []
```

## Decision

`@family::name` defines a member of a resource family. Methods use callable
bodies, properties use value bindings, and receiver references remain scoped
to the family definition.

## Evolution Boundary

Capture, dynamic dispatch, inheritance, or broader inlined-body shapes must
define ownership and receiver lifetime before extending this feature.
