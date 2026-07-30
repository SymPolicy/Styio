# Effect-Aware Callable Generalization

**Purpose:** Own the closed-and-pure eligibility rule that separates generalized callable relations from effectful or capture-dependent monomorphic callables.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.effect-aware-callable-generalization"
title = "Effect-Aware Callable Generalization"
kind = "callable-type-semantics"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / Type System"
syntax = "No new source syntax proposed; this decision governs eligibility of final callable bindings for generalization."
resolution = "Generalize only closed, proven-pure final callable bodies; unknown or effectful bodies remain monomorphic, while inferred effect rows are reserved for a later feature."
golden_cases = ["tests/features/callable_effects/t02_transitive_effect_summary.styio", "tests/features/callable_effects/e02_captured_environment_second_instance.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]
implementation = ["src/StyioSema/TypeInfer.cpp", "src/StyioSema/SemaContext.hpp"]
evidence = ["tests/features/callable_effects/t02_transitive_effect_summary.styio", "tests/features/callable_effects/e01_effectful_second_instance.styio", "tests/features/callable_effects/e02_captured_environment_second_instance.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_effects/t02_transitive_effect_summary.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
effect-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "prepare_callable_type_schemes"
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = []
```

## Decision

Styio adopts a two-stage rule. A call-graph member is generalizable only when
its free environment is closed and every reachable operation is proven pure.
Mutable captures, resource operations, task operations, fallback/handler
operations, unclassified native calls, and unknown callee summaries fail
closed to the existing monomorphic callable path.

Compiler-owned inferred effect rows remain an explicit later extension. They
must not be simulated by syntactic exceptions or implicit trust annotations.

## Prerequisite Boundary

The semantic pass computes one deterministic summary per final callable,
propagates summaries through direct-call dependencies to a fixed point, and
admits relation inference only for summaries that remain both closed and pure.
The active closed effect vocabulary covers output, resource, task, handler,
native, capture, and unknown operations. Unsupported nodes and unresolved
callees are classified as unknown rather than assumed pure.

An effectful or capture-dependent callable stays on the monomorphic path. Its
first checked concrete argument vector fixes the instance; a later conflicting
use reports the canonical effect summary and the parameter conflict before
lowering.

The positive and negative goldens prove transitive output propagation,
single-instance execution, direct output rejection on a second type, and
capture-dependent rejection on a second type.

## Evolution Boundary

This feature does not decide handler syntax, effect-row source notation, native
purity annotations, or resource capability polymorphism.
