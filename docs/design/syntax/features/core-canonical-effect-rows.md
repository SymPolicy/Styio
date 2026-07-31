# Canonical Effect Rows

**Purpose:** Own the compiler/interface representation of callable effects as deterministic rows, including the future open-tail fact needed by higher-order relations, without adding authored effect syntax.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.canonical-effect-rows"
title = "Canonical Effect Rows"
kind = "callable-effect-semantics"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Sema / Modules"
syntax = "No new source form; canonical effect rows are compiler-owned scheme, diagnostic, typed-IR, and `.styioi` facts."
resolution = "Replace the closed bit-summary identity with a canonical row of known labels plus an optional compiler-owned open tail for higher-order relations; keep native unknown effects fail-closed and do not accept source assertions."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/SemaContext.hpp", "src/StyioSema/TypeInfer.cpp", "src/StyioSema/CallableInterface.cpp"]
evidence = ["tests/features/callable_effects/t02_transitive_effect_summary.styio"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_effects/t02_transitive_effect_summary.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
interface-contract = "docs/design/syntax/features/core-callable-interface-scheme-publication.md"

[implementation]
owner = "Sema / Modules"

[dependencies]
requires = [
  { id = "core.effect-aware-callable-generalization", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.callable-interface-scheme-publication", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.effect-aware-callable-generalization"]
conflicts = []
supersedes = []
after = ["core.callable-interface-scheme-publication"]
```

## Decision

Q4-A was approved on 2026-07-31. Every callable effect identity is a canonical
row. Known labels are sorted and deduplicated from the closed vocabulary
`output`, `resource`, `task`, `handler`, `native`, `capture`, and `unknown`.
A row may also carry one compiler-owned open tail variable, but the tail is
created only by a checked higher-order relation; ordinary first-order bodies
produce closed rows.

The row is serialized in callable schemes and module interfaces and
participates in their semantic/ABI digests. The existing effect bits may remain
an internal acceleration cache, but they are derived from the row and are not
the identity authority.

## Semantic and Diagnostic Boundary

Effect propagation unions known labels and unifies compatible open tails.
Unclassified native calls and unsupported nodes contribute `unknown`;
`unknown` is never erased by an annotation or trusted as pure. Generalization
still requires a closed empty row.

Diagnostics print one canonical row and, when present, its open tail. Source
authors are not asked to write row variables or purity assertions.

## Compatibility and Evolution Boundary

Pure and closed-effect programs retain their source behavior. Interface schema
changes are versioned and stale older metadata fails closed.

Source-visible effect rows, native purity assertions, handler abstraction,
effect subtyping, row subtraction syntax, and user-defined effect labels
require separate child decisions.
