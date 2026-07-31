# Rank-2 Callback Polymorphism

**Purpose:** Preserve the approved long-term boundary for context-checked generalized callback parameters without enabling inferred impredicative callable values.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.rank2-callback-polymorphism"
title = "Rank-2 Callback Polymorphism"
kind = "higher-rank-callable-semantics"
decision_state = "deferred"
delivery_state = "not_started"
owner = "Sema / Type System"
syntax = "No source `forall` or generic binder is selected; a future higher-order parameter may carry one compiler-owned generalized callback requirement."
resolution = "Reserve bidirectional, context-checked rank-2 callback parameters as the only approved higher-rank direction; do not infer impredicative lists, dictionaries, fields, captures, or arbitrary generalized values."
reopen_when = "The original Q1/Q2/Q4/Q5 delivery floor converged on 2026-07-31; reopen implementation only after the owner resolves queued questions Q3.1-Q3.3 covering contract origin, static representation, and shallow pure subsumption."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/TypeInfer.cpp"]
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
typed-ir-boundary = "docs/rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md"

[implementation]
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.monomorphic-callable-values", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.affine-capturing-closures", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.canonical-effect-rows", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.capability-usage-polymorphism", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.higher-order-callable-polymorphism"]
conflicts = []
supersedes = []
after = ["core.affine-capturing-closures"]
```

## Decision

Q3-A was approved as a deferred direction on 2026-07-31. When reopened, the
first higher-rank surface is a callback parameter whose required generalized
relation is supplied by its enclosing callable contract and checked
bidirectionally. A named function may satisfy that requirement without turning
the function into an impredicative runtime value.

There is no source `forall`, no inferred higher-rank result, and no
generalized callable stored in a list, dictionary, field, capture, or ordinary
binding.

## Reopen Status and Researched Implementation Direction

The original dependency floor is now satisfied: monomorphic callable values,
affine static closures, canonical effect rows, capability/usage facts,
portable typed bodies, and content-addressed specialization all have executable
evidence. Dependency convergence does not auto-approve implementation.

The primary-source refresh and the still-unanswered owner batch live in
[Callable Type Evolution Questions](../CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md#queued-follow-up-question-set).
GHC and OCaml both require an expected higher-rank contract rather than
inferring universal quantification from ordinary callback applications; GHC
also documents that deep subsumption can insert eta-expansion and change
evaluation behavior. Rust scopes its implemented higher-ranked bounds
explicitly, while Scala distinguishes polymorphic function values with an
explicit type-level parameter boundary.

The recommended Styio-specific first slice is therefore:

1. receive one nested generalized callback scheme from a compiler-owned or
   standard-module interface contract before selecting source authoring syntax;
2. accept only a final noncapturing named callable item as a compile-time
   scheme witness;
3. specialize the enclosing callable by that item's semantic identity and
   erase the generic callback from the runtime ABI;
4. instantiate every callback use as an ordinary concrete mono item and include
   its scheme, demand set, portable body, dependencies, effects, usage facts,
   and item identity in the enclosing specialization digest;
5. use shallow top-level scheme subsumption with a closed empty effect row and
   no implicit eta-expansion, generalized capture, higher-rank result, or
   polymorphic container.

Those five points are recommendations only until Q3.1–Q3.3 are answered.

## Diagnostic Boundary

A future checker must distinguish “monomorphic callback type mismatch” from
“callback does not satisfy the required generalized relation.” It must never
repair an ambiguous expression by guessing a rank or by generalizing a mutable
or capturing value. A rejected scheme witness should identify the required
nested relation, the first incompatible concrete instance, and any effect or
usage fact that prevented subsumption without inventing source `forall`
guidance.

## Compatibility and Evolution Boundary

The current direct-call rank-1 behavior and Q1 monomorphic values remain
unchanged. Fully impredicative values, polymorphic fields/containers, inferred
higher-rank arguments, and source generic binders remain unapproved.
