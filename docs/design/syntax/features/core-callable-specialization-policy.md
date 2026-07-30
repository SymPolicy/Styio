# Callable Specialization Policy

**Purpose:** Decide reachability, ownership, caching, linkage, and growth controls for concrete callable instances.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.callable-specialization-policy"
title = "Callable Specialization Policy"
kind = "generic-codegen-policy"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Lowering / Codegen"
syntax = "No source-level explicit-instantiation syntax proposed."
resolution = "Collect only reachable instances, assign deterministic single-owner symbols, key reuse by canonical semantic and backend digests, and fail closed at recursive or pathological growth ceilings."
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
  { id = "core.callable-interface-scheme-publication", decision_state = "accepted", delivery_state = "not_started" },
]
requires_any = []
extends = ["core.context-driven-call-instantiation"]
conflicts = []
supersedes = []
after = ["core.callable-interface-scheme-publication"]
```

## Decision

Normal builds collect a lazy reachable mono-item graph. Each instance receives
one deterministic owner and a content-addressed identity derived from its
canonical relation, checked body, dependencies, target, and ABI facts. A hard
recursive-instantiation ceiling and a high pathological-growth safety ceiling
fail with an instance-path diagnostic. A normal code-size warning threshold
remains telemetry-driven rather than part of the language contract.

## Runtime Boundary

Schemes and constraints remain compile-time facts. This feature must not add
boxing, witness tables, dynamic type dictionaries, or GC to generated code.

## Evolution Boundary

Profile-guided eager instances, distributed caches, explicit source
instantiation, and stable function-address identity require separate decisions.
