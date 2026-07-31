# Portable Generic Body Interface

**Purpose:** Own the long-term split between a stable public callable scheme and a versioned, independently verifiable typed-body payload used for downstream specialization.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.portable-generic-body-interface"
title = "Portable Generic Body Interface"
kind = "module-generic-interface"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Sema / Modules / StyioIR"
syntax = "No new source form; this is a versioned compiler artifact split behind existing import/export syntax."
resolution = "Publish a stable relation/effect/capability contract separately from a canonical typed StyioIR payload; consumers verify schema, semantic body, dependency, compiler, and target-independent digests before specialization."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/CallableInterface.cpp", "src/StyioSema/CallableModuleLoader.cpp", "src/StyioIR/Verifier.cpp"]
evidence = ["tests/features/callable_interfaces/t01_downstream_specialization.styio"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_interfaces/t01_downstream_specialization.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
typed-ir-contract = "docs/rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md"
interface-contract = "docs/design/syntax/features/core-callable-interface-scheme-publication.md"

[implementation]
owner = "Sema / Modules / StyioIR"

[dependencies]
requires = [
  { id = "core.callable-interface-scheme-publication", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.canonical-effect-rows", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.capability-usage-polymorphism", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.callable-interface-scheme-publication"]
conflicts = []
supersedes = []
after = ["core.canonical-effect-rows", "core.capability-usage-polymorphism"]
```

## Decision

Q7-A was approved as a blocked long-term direction on 2026-07-31. The public
scheme is the stable compatibility contract. A separate versioned payload
contains canonical typed StyioIR sufficient to reproduce a checked generic
body without reparsing defining source.

A consumer verifies the scheme, effect/capability facts, typed-body schema and
digest, direct dependency identities, target-independent semantic digest, and
compiler ABI before admitting the payload. The verifier rejects unknown nodes,
unbound symbols, mismatched types, stale dependencies, and noncanonical
serialization.

## Blocked Delivery Boundary

Implementation waits for canonical effect rows and capability/usage facts so
the stable scheme cannot omit semantic obligations carried by the body. The
current `.styioi` checked-body representation remains authoritative until this
child converges.

Cross-module recursive SCCs remain rejected. A later whole-program module graph
must establish one owner and fixed point before that boundary can change.

## Diagnostic and Compatibility Boundary

Diagnostics distinguish stable scheme incompatibility from optional typed-body
payload invalidity. An invalid payload fails closed; it never falls back to
trusting source text or opaque native code.

Non-generic exports retain their concrete ABI. Native generic ABI stability,
binary-only generic libraries, source-only reparsing, dynamic loading, and
cross-module mutual recursion are not approved here.
