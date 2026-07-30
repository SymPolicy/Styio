# Callable Interface Scheme Publication

**Purpose:** Decide the module-interface facts and generic-body availability required for separately compiled callers to instantiate an exported inferred callable safely.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.callable-interface-scheme-publication"
title = "Callable Interface Scheme Publication"
kind = "module-type-interface"
decision_state = "review"
delivery_state = "not_started"
owner = "Sema / Modules"
syntax = "Existing callable export/import forms; no authored generic interface syntax proposed."
resolution = "Owner review pending: choose published scheme/body/effect/ABI metadata and define which compilation unit owns downstream instances."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]

[prerequisites]
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"
module-contract = "docs/design/Styio-EBNF.md"

[implementation]

[dependencies]
requires = [
  { id = "core.import-declaration", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.effect-aware-callable-generalization", decision_state = "review", delivery_state = "not_started" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = ["core.effect-aware-callable-generalization"]
```

## Decision Needed

Choose whether an exported generic interface contains only a canonical scheme
or also a checked typed body, effect/capability summary, dependency hashes, and
ABI/lowering facts needed for downstream specialization.

The decision must also define behavior for an exported generic callable with no
local call sites and for recursive edges that cross module boundaries.

## Recommendation

Publish the canonical scheme, checked typed-body representation or equivalent
reproducible body, effect/capability summary, and stable dependency/ABI digests.
Validate the body in the defining module even when unused. Let the consuming
compilation unit specialize it, while a deterministic ownership/linkage policy
deduplicates instances. Reject cross-module recursive SCCs in the first slice.

## Compatibility Boundary

Non-generic exports preserve their existing concrete symbol and interface
facts. Generic metadata must be versioned so stale interfaces fail closed.

## Evolution Boundary

Opaque generic bodies, whole-program SCCs, binary-only generic libraries, and
stable public generic ABI require separate decisions.
