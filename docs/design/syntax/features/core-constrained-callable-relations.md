# Constrained Callable Relations

**Purpose:** Decide how inferred callable variables record requirements introduced by operators, comparisons, collection operations, and capabilities that plain equality unification cannot express.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.constrained-callable-relations"
title = "Constrained Callable Relations"
kind = "callable-type-semantics"
decision_state = "review"
delivery_state = "not_started"
owner = "Sema / Type System"
syntax = "No source constraint syntax proposed; constraints would initially be inferred from ordinary expressions."
resolution = "Owner review pending: choose closed compiler-owned capability constraints or an open user-extensible trait/type-class system."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]

[prerequisites]
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
capability-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"

[implementation]

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.type-expression", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = []
```

## Decision Needed

Choose the constraint vocabulary and its coherence owner for relations such as:

```styio
# double := (value) => value + value
# first := (items) => items[0]
```

Plain HM equality cannot infer these definitions without knowing which types
support `+` or indexing.

## Recommendation

Begin with a closed compiler-owned set derived from existing type and capability
facts: numeric, comparable, indexable, iterable, cloneable, and any future
resource-specific facts approved elsewhere. Keep constraints in semantic
metadata and diagnostics; do not add user-defined instances or source-level
trait declarations in this feature.

## Prerequisite Boundary

Delivery requires deterministic constraint normalization, satisfiability
checking at instantiation, coherent operator selection, and stable diagnostics.
Numeric conversion and literal defaulting remain separate decisions.

## Evolution Boundary

Open instances, orphan/coherence rules, associated types, specialization among
instances, and constraint syntax require child feature SSOTs.
