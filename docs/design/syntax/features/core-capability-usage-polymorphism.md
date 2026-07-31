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
delivery_state = "converged"
owner = "Sema / Resource Topology"
syntax = "No authored lifetime, capability, multiplicity, or shape variable; all usage/capability facts are compiler-owned relation metadata."
resolution = "Admit capability-sensitive relation variables only through closed compiler-owned copy/borrow/consume, task-transfer, resource-state-family, and materialized-shape facts, with each family enabled separately and revalidated against topology at every concrete instance."
golden_cases = ["tests/features/callable_capabilities/t02_usage_instances.styio", "tests/features/callable_capabilities/e07_copy_task_instance.styio", "tests/features/callable_interfaces/t03_usage_facts.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md", "docs/design/Styio-Resource-Topology.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/CallableUsage.hpp", "src/StyioSema/SemaContext.hpp", "src/StyioSema/TypeInfer.cpp", "src/StyioSema/CallableInterface.cpp"]
evidence = ["tests/styio_test.cpp", "tests/features/callable_capabilities/t01_scalar_collection_instances.styio", "tests/features/callable_capabilities/t02_usage_instances.styio", "tests/features/callable_capabilities/e01_matrix_instance.styio", "tests/features/callable_capabilities/e02_task_instance.styio", "tests/features/callable_capabilities/e03_stream_instance.styio", "tests/features/callable_capabilities/e04_file_instance.styio", "tests/features/callable_capabilities/e05_nested_matrix_collection.styio", "tests/features/callable_capabilities/e06_topology_resource_instance.styio", "tests/features/callable_capabilities/e07_copy_task_instance.styio", "tests/features/callable_capabilities/e08_topology_expected_collection.styio", "tests/features/callable_interfaces/t03_usage_facts.styio"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_capabilities/t02_usage_instances.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
ownership-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "validate_callable_usage_instance"
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

## Implemented First Stage

`CallableUsageSet` is the canonical identity for the closed `consume`, `copy`,
`exclusive_borrow`, and `shared_borrow` vocabulary. It insert-sorts and
deduplicates enum values and derives its textual form; no bit mask or cached
canonical string is authoritative.

For the current pure principal-relation subset, Sema derives shared borrow from
parameter reads, copy from repeated use, and consume from returned, stored, or
otherwise escaping values. Exact parameter-to-parameter direct-call edges
propagate callee facts to callers to a fixed point, including recursive
components. `exclusive_borrow` is represented, serialized, and validated, but
the current pure subset contains no mutation form that may emit it; adding
such an emitter requires the owning mutation feature's executable evidence.

Requirements are attached to normalized relation variables in sorted order.
`.styioi` schema v4 serializes each variable and its sorted usage list and
rejects schema v3 rather than maintaining a dual reader. Usage facts are part
of the canonical relation, interface ABI, checked-definition dependency
fingerprints, and specialization identity.

Instance validation retains the original concrete `StyioDataType`. It reports
the first incompatible fact in stable order, distinguishing `copy`,
`exclusive_borrow`, `consume`, `task_transfer`, `resource_state_family`,
`topology_identity`, and `materialized_shape`. The scalar/list/dict domain
continues to execute; repeated task use proves alias/copy rejection, while
task, stream, file, topology, matrix, and nested-matrix fixtures prove that
disabled fact families stay closed. A conflicting plain expected result also
cannot coerce a topology resource into its collection representation.

This convergence delivers the first-stage relation vocabulary, propagation,
interface contract, and instance revalidation. It does not admit task,
resource, topology, or matrix handles into generalized variables.

## Compatibility and Evolution Boundary

The existing plain scalar/list/dict generalization domain remains valid.
Handle families stay monomorphic until their required fact family is enabled.

Authored lifetimes, capability subtyping, generic resource methods,
unconstrained shape arithmetic, implicit copying, and an open user capability
vocabulary are not approved here.
