# Affine Capturing Closures

**Purpose:** Own the capture-ownership and escape rules for callable values whose explicit `$(...)` environment is derived as borrow or consume rather than copied or garbage-collected implicitly.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.affine-capturing-closures"
title = "Affine Capturing Closures"
kind = "closure-ownership"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Sema / Resource Topology"
syntax = "Reuse the existing explicit `$(name, ...)` capture list; no lifetime, copy, heap, or ownership keyword is introduced."
resolution = "Derive shared-borrow, exclusive-borrow, or consume facts from each captured use; permit stack/static nonescaping environments and permit escape only when every capture has a deterministic owned representation and drop path."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Handle-Capability-Type-System.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/TypeInfer.cpp", "src/StyioResourceTopology/ResourceTopology.cpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/README.md"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/README.md"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
ownership-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
owner = "Sema / Resource Topology"

[dependencies]
requires = [
  { id = "core.monomorphic-callable-values", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.canonical-effect-rows", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.capability-usage-polymorphism", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.monomorphic-callable-values"]
conflicts = []
supersedes = []
after = ["core.canonical-effect-rows", "core.capability-usage-polymorphism"]
```

## Decision

Q2-A was approved as a blocked direction on 2026-07-31. Styio reuses the
visible `$(...)` capture list and derives how each capture is used. A read-only
capture is a shared borrow, a mutation requires an exclusive borrow, and a
value moved into an escaping environment is consumed.

The environment is affine: it may be invoked according to the checked callable
use contract, but it is never assumed freely copyable. Nonescaping
environments may remain stack/static. Escape is legal only when every captured
field has a deterministic owned representation, transfer rule, and drop path.

## Blocked Delivery Boundary

Implementation cannot start until monomorphic callable values, canonical
effect rows, and compiler-owned capability/usage facts have converged. The
dependency graph, rather than this prose, determines when that floor is met.

Resource, stream, task, and topology handles must retain their existing state
and ordering identities inside an environment. Capturing does not normalize
them to a shared integer representation.

## Diagnostic and Compatibility Boundary

Diagnostics identify the capture, derived use mode, escape point, and missing
transfer/drop fact. They must reject aliasing an exclusive borrow, using a
consumed capture, or allowing a borrowed value to outlive its checked scope
before lowering.

Existing reactive capture syntax remains valid for its current monomorphic
semantics. This feature does not silently make an existing capture escaping,
copyable, reference-counted, or heap allocated.

## Evolution Boundary

Authored lifetime names, reference counting, cyclic closure environments,
implicit capture discovery, generalized capturing closures, and a garbage
collector are not approved by this feature.
