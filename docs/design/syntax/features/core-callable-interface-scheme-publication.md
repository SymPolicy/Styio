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
delivery_state = "converged"
owner = "Sema / Modules"
syntax = "Existing callable export/import forms; no authored generic interface syntax proposed."
resolution = "Publish the canonical scheme, checked typed body, canonical effect row/capability facts, and stable dependency/ABI digests; downstream units specialize, and cross-module recursive SCCs are rejected."
golden_cases = ["tests/features/callable_interfaces/t01_downstream_specialization.styio", "tests/features/callable_interfaces/t02_effect_rows.styio", "tests/features/callable_interfaces/t03_usage_facts.styio", "tests/features/callable_interfaces/e03_stale_source.styio", "tests/features/callable_interfaces/e05_stale_dependency.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]
implementation = ["src/StyioSema/CallableInterface.cpp", "src/StyioSema/CallableModuleLoader.cpp", "src/StyioSema/TypeInfer.cpp", "src/StyioLowering/AstToStyioIR.cpp", "src/main.cpp"]
evidence = ["tests/features/callable_interfaces/t01_downstream_specialization.styio", "tests/features/callable_interfaces/t02_effect_rows.styio", "tests/features/callable_interfaces/t03_usage_facts.styio", "tests/features/callable_interfaces/e01_private_callable.styio", "tests/features/callable_interfaces/e02_cross_module_cycle.styio", "tests/features/callable_interfaces/e03_stale_source.styio", "tests/features/callable_interfaces/e04_stale_schema.styio", "tests/features/callable_interfaces/e05_stale_dependency.styio", "tests/features/callable_interfaces/e06_missing_interface.styio", "tests/features/callable_interfaces/modules/invalid.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_interfaces/t01_downstream_specialization.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"
module-contract = "docs/design/Styio-EBNF.md"

[implementation]
path = "src/StyioSema/CallableInterface.cpp"
symbol = "publish_callable_module_interface"
owner = "Sema / Modules"

[dependencies]
requires = [
  { id = "core.import-declaration", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.effect-aware-callable-generalization", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = ["core.effect-aware-callable-generalization"]
```

## Decision

An exported generic interface publishes its canonical scheme, checked typed
body representation or equivalent reproducible body, canonical effect row and
capability facts, and stable dependency and ABI digests. The defining
module validates the body even without local instances. A consuming compilation
unit may specialize it under deterministic ownership. Cross-module recursive
SCCs are rejected in this slice.

## Implemented Boundary

The compiler can publish a sibling callable interface with
`--file=<module>.styio --module-id=<canonical/path>
--emit-module-interface=<module>.styioi`. Schema v3 records the canonical
relation and normalized constraints, a sorted effect-label row with a nullable
compiler-owned open tail, sorted per-variable usage requirements, concrete
signatures, a deterministic checked-body representation and SHA-256 digest,
the source digest, direct dependency module set and digest, compiler ABI facts,
and the resulting interface ABI digest.

Import loading is recursive and fail-closed. A module source and its sibling
`.styioi` must both exist. The loader validates schema, module identity,
compiler ABI, source content, direct dependency identities and digests,
checked-body digests, and the recomputed interface ABI before installing any
callable facts. The entry module can see only exported callables from its
direct imports; imported bodies retain access to their own private helpers and
to exported callables of their direct dependencies. Name collisions and
duplicate imports are rejected deterministically.

All checked callable definitions needed to reproduce imported bodies are kept
in the interface, including private generic or concrete helpers. Only exported
entries are visible to consumers. The defining compilation checks exported
bodies even when it has no local runtime instance, while concrete exports must
publish complete parameter and result facts.

## Safety Boundary

Import declarations lower to a no-op only after the module graph has resolved
and validated every required interface. Missing, stale, malformed, or
ABI-incompatible metadata therefore fails during the type phase and cannot
fall through to code generation.

The current implementation rejects every cross-module dependency cycle as a
conservative boundary for the approved cross-module recursive-SCC rejection.
It resolves slash-form imports relative to the importing source and does not
write dependency interfaces implicitly. Build orchestration must publish each
dependency explicitly before compiling a consumer.

## Compatibility Boundary

Non-generic exports preserve their existing concrete symbol and interface
facts. Generic metadata must be versioned so stale interfaces fail closed.
The schema v3 cutover deliberately rejects v2 metadata rather than retaining a
dual-reader compatibility path.

## Evolution Boundary

Opaque generic bodies, whole-program SCCs, binary-only generic libraries, and
stable public generic ABI require separate decisions.
