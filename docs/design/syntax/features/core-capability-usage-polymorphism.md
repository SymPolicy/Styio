# Capability and Usage Polymorphism

**Purpose:** Own the staged compiler facts that allow selected non-plain values to participate in inferred relations without erasing ownership, use count, transfer safety, state family, or materialized shape.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.capability-usage-polymorphism"
title = "Capability and Usage Polymorphism"
kind = "capability-type-semantics"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Sema / Resource Topology"
syntax = "No authored lifetime, capability, multiplicity, or shape variable; all usage/capability facts are compiler-owned relation metadata."
resolution = "Admit capability-sensitive relation variables only through closed compiler-owned copy/borrow/consume, task-transfer, resource-state-family, and materialized-shape facts, with each family enabled separately and revalidated against topology at every concrete instance."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md", "docs/design/Styio-Resource-Topology.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/SemaContext.hpp", "src/StyioSema/TypeInfer.cpp", "src/StyioResourceTopology/ResourceTopology.cpp"]
evidence = ["tests/features/callable_capabilities/t01_scalar_collection_instances.styio"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_capabilities/t01_scalar_collection_instances.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
ownership-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
owner = "Sema / Resource Topology"

[dependencies]
requires = [
  { id = "core.capability-polymorphic-handles", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.canonical-effect-rows", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.capability-polymorphic-handles"]
conflicts = []
supersedes = []
after = ["core.canonical-effect-rows"]
```

## Decision

Q5-A was approved as a staged direction on 2026-07-31. The first stage adds
compiler-owned copy, shared-borrow, exclusive-borrow, and consume facts to
relation variables. A later stage may add task-transfer safety; resource state
families and materialized matrix shape each require their own evidence before
admission.

Representation equality is never sufficient. Every concrete instance is
checked against the original `StyioDataType` and resource-topology identity
before normalization. A relation cannot substitute one file, task, stream,
matrix, or resource handle merely because both lower to the same LLVM family.

## Delivery and Diagnostic Boundary

Delivery starts only after canonical effect rows converge. Each admitted fact
family must have positive execution evidence and negative alias, consume,
state, topology, and shape evidence before it can be marked converged.

Diagnostics name the relation variable, required usage/capability facts, the
concrete candidate, and the first incompatible fact. They must not collapse
the error to a representation mismatch.

## Compatibility and Evolution Boundary

The existing plain scalar/list/dict generalization domain remains valid.
Handle families stay monomorphic until their required fact family is enabled.

Authored lifetimes, capability subtyping, generic resource methods,
unconstrained shape arithmetic, implicit copying, and an open user capability
vocabulary are not approved here.
