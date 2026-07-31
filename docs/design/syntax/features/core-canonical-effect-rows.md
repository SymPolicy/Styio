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
delivery_state = "converged"
owner = "Sema / Modules"
syntax = "No new source form; canonical effect rows are compiler-owned scheme, diagnostic, typed-IR, and `.styioi` facts."
resolution = "Replace the closed bit-summary identity with a canonical row of known labels plus an optional compiler-owned open tail for higher-order relations; keep native unknown effects fail-closed and do not accept source assertions."
golden_cases = [
  "tests/features/callable_effects/t02_transitive_effect_summary.styio",
  "tests/features/callable_effects/e01_effectful_second_instance.styio",
  "tests/features/callable_effects/e02_captured_environment_second_instance.styio",
  "tests/features/callable_interfaces/t02_effect_rows.styio",
]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/EffectRow.hpp", "src/StyioSema/SemaContext.hpp", "src/StyioSema/TypeInfer.cpp", "src/StyioSema/CallableInterface.hpp", "src/StyioSema/CallableInterface.cpp", "src/StyioIR/GenIR/SGIR.hpp", "src/StyioLowering/AstToStyioIR.cpp", "src/main.cpp"]
evidence = ["tests/features/callable_effects/t02_transitive_effect_summary.styio", "tests/features/callable_effects/e01_effectful_second_instance.styio", "tests/features/callable_effects/e02_captured_environment_second_instance.styio", "tests/features/callable_interfaces/t02_effect_rows.styio"]

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
path = "src/StyioSema/EffectRow.hpp"
symbol = "class CallableEffectRow"
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

## Delivered Contract

`CallableEffectRow` is the single semantic value for callable effects. It owns
an insertion-sorted, deduplicated vector from the closed label vocabulary and
one optional compiler tail. The canonical form is derived on demand:
`{}` is closed and pure, `{output,resource}` is closed and effectful, and
`{|'e0}` is an open higher-order row. No stored bit mask or canonical string
can become a competing identity.

Sema creates an open tail only when a checked callable parameter is invoked,
unions known labels through direct-call dependencies, and normalizes a
propagated open tail to the caller's local tail variable. Captures are the
ordinary closed label `capture`; capture names remain diagnostic facts rather
than row identity. Generalization continues to require exactly `{}`.

Callable interface schema v3 serializes sorted `labels` and nullable
`open_tail` alongside the separately owned usage requirements, derives
canonical identity for ABI and specialization hashes, and rejects schema v2
before installing metadata. `SGFunc` carries the same row
through typed IR; manually constructed functions default to `{unknown}` so
missing Sema facts fail closed.

Acceptance evidence covers row canonicalization and deduplication, transitive
closed effects, open-tail propagation across a direct-call edge, interface
publication/consumption, old-schema rejection, and row-based monomorphism
diagnostics. The focused commands are:

```bash
build/bin/styio_test \
  --gtest_filter=StyioCallableEffects.CanonicalRowsSortDeduplicateAndMerge
ctest --test-dir build -L callable_effects --output-on-failure --no-tests=error
ctest --test-dir build -L callable_interfaces --output-on-failure --no-tests=error
```
