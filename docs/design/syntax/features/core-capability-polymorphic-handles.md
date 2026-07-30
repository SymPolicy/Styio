# Capability-Polymorphic Handles

**Purpose:** Decide whether inferred callable variables may range over resource, stream, task, matrix, and other ownership- or representation-sensitive handle families.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.capability-polymorphic-handles"
title = "Capability-Polymorphic Handles"
kind = "capability-type-semantics"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Sema / Resource Topology"
syntax = "No new source form proposed; this decision governs the universe of inferred relation variables."
resolution = "Generalized variables admit plain immutable scalar values and pure materialized collections only; resource, stream, task, matrix, and ownership-sensitive handles remain monomorphic."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md", "docs/design/Styio-Resource-Topology.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]

[prerequisites]
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
capability-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.effect-aware-callable-generalization", decision_state = "accepted", delivery_state = "not_started" },
  { id = "core.constrained-callable-relations", decision_state = "accepted", delivery_state = "not_started" },
  { id = "resource.slot-declaration", decision_state = "accepted", delivery_state = "converged" },
  { id = "task.single-task", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = ["core.effect-aware-callable-generalization", "core.constrained-callable-relations"]
```

## Decision

Generalized variables initially admit plain immutable scalar values and pure
materialized lists and dictionaries. Resources, streams, tasks, matrices, and
ownership-sensitive handles remain monomorphic until capability, effect, and
linearity facts participate in schemes and resource topology can validate each
concrete instance.

## Safety Boundary

Generic substitution must never erase a consume-once transition, turn a borrow
into ownership, make a non-sendable value cross a task boundary, or route one
handle family through another family's LLVM/runtime representation.

## Evolution Boundary

Lifetime polymorphism, linear/affine callable variables, capability subtyping,
shape polymorphism, and generic resource methods require separate decisions.
