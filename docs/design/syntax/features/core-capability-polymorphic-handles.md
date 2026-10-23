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
delivery_state = "converged"
owner = "Sema / Resource Topology"
syntax = "No new source form proposed; this decision governs the universe of inferred relation variables."
resolution = "Generalized variables admit plain immutable scalar values and pure materialized collections only; resource, stream, task, matrix, and ownership-sensitive handles remain monomorphic."
golden_cases = ["tests/features/callable_capabilities/t01_scalar_collection_instances.styio", "tests/features/callable_capabilities/e06_topology_resource_instance.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md", "docs/design/Styio-Resource-Topology.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]
implementation = ["src/StyioSema/TypeInfer.cpp"]
evidence = ["tests/features/callable_capabilities/t01_scalar_collection_instances.styio", "tests/features/callable_capabilities/e01_matrix_instance.styio", "tests/features/callable_capabilities/e02_task_instance.styio", "tests/features/callable_capabilities/e03_stream_instance.styio", "tests/features/callable_capabilities/e04_file_instance.styio", "tests/features/callable_capabilities/e05_nested_matrix_collection.styio", "tests/features/callable_capabilities/e06_topology_resource_instance.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_capabilities/t01_scalar_collection_instances.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
capability-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "callable_generalization_domain_accepts"
owner = "Sema / Resource Topology"

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.effect-aware-callable-generalization", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.constrained-callable-relations", decision_state = "accepted", delivery_state = "converged" },
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

## Implemented Boundary

Scheme matching uses a recursive closed-domain predicate. It admits boolean,
integer, floating, decimal, character, and string scalar representations with
no handle state or capabilities. It also admits exact materialized `list` and
`dict` families only when their element, key, and value types recursively stay
inside the same domain.

The predicate inspects the original concrete type before callable type
normalization. A topology resource or sequence therefore cannot lose its
resource shape and reappear as an ordinary `list[T]`. Matrix, task, file,
stream, topology-resource, range-handle, user-defined, and nested
capability-sensitive instances fail before specialization or lowering.

## Safety Boundary

Generic substitution must never erase a consume-once transition, turn a borrow
into ownership, make a non-sendable value cross a task boundary, or route one
handle family through another family's LLVM/runtime representation.

The stable diagnostic names the inferred relation variable, concrete rejected
type, and matching context, then states the closed admitted domain. The
`callable_capabilities` goldens pair scalar/list/dict execution with matrix,
task, stream, file, nested-matrix collection, and topology-resource failures.

## Evolution Boundary

Lifetime polymorphism, linear/affine callable variables, capability subtyping,
shape polymorphism, and generic resource methods require separate decisions.
