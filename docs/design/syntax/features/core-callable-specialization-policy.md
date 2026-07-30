# Callable Specialization Policy

**Purpose:** Decide reachability, ownership, caching, linkage, and growth controls for concrete callable instances.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.callable-specialization-policy"
title = "Callable Specialization Policy"
kind = "generic-codegen-policy"
decision_state = "review"
delivery_state = "not_started"
owner = "Lowering / Codegen"
syntax = "No source-level explicit-instantiation syntax proposed."
resolution = "Owner review pending: choose lazy/eager collection, deterministic instance ownership, cache keys, and failure behavior for excessive instance growth."
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
backend-contract = "docs/specs/AGENT-SPEC.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"

[implementation]

[dependencies]
requires = [
  { id = "core.context-driven-call-instantiation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.callable-interface-scheme-publication", decision_state = "review", delivery_state = "not_started" },
]
requires_any = []
extends = ["core.context-driven-call-instantiation"]
conflicts = []
supersedes = []
after = ["core.callable-interface-scheme-publication"]
```

## Decision Needed

Choose:

1. lazy, eager, or hybrid instance collection;
2. the compilation unit that owns an instance;
3. which scheme/body/backend facts enter the cache and symbol key; and
4. warning and failure behavior for recursive or combinatorial instance growth.

## Recommendation

Use a lazy reachable mono-item graph for normal builds, deterministic
single-owner placement, and content-addressed reuse keyed by the canonical
relation plus checked body, dependency, target, and ABI digests. Add a hard
recursive-instantiation ceiling and a high pathological-growth safety ceiling
with an instance-path diagnostic. Gather telemetry before fixing a normal
code-size warning threshold.

## Runtime Boundary

Schemes and constraints remain compile-time facts. This feature must not add
boxing, witness tables, dynamic type dictionaries, or GC to generated code.

## Evolution Boundary

Profile-guided eager instances, distributed caches, explicit source
instantiation, and stable function-address identity require separate decisions.
