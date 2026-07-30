# Callable Interface Scheme Publication

**Purpose:** Decide the module-interface facts and generic-body availability required for separately compiled callers to instantiate an exported inferred callable safely.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.callable-interface-scheme-publication"
title = "Callable Interface Scheme Publication"
kind = "module-type-interface"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Sema / Modules"
syntax = "Existing callable export/import forms; no authored generic interface syntax proposed."
resolution = "Publish the canonical scheme, checked typed body, effect/capability summary, and stable dependency/ABI digests; downstream units specialize, and cross-module recursive SCCs are rejected."
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
  { id = "core.effect-aware-callable-generalization", decision_state = "accepted", delivery_state = "not_started" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = ["core.effect-aware-callable-generalization"]
```

## Decision

An exported generic interface publishes its canonical scheme, checked typed
body representation or equivalent reproducible body, normalized
effect/capability summary, and stable dependency and ABI digests. The defining
module validates the body even without local instances. A consuming compilation
unit may specialize it under deterministic ownership. Cross-module recursive
SCCs are rejected in this slice.

## Compatibility Boundary

Non-generic exports preserve their existing concrete symbol and interface
facts. Generic metadata must be versioned so stale interfaces fail closed.

## Evolution Boundary

Opaque generic bodies, whole-program SCCs, binary-only generic libraries, and
stable public generic ABI require separate decisions.
