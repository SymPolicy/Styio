# Portable Generic Body Interface

**Purpose:** Own the long-term split between a stable public callable scheme and a versioned, independently verifiable typed-body payload used for downstream specialization.

**Last updated:** 2026-08-22

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.portable-generic-body-interface"
title = "Portable Generic Body Interface"
kind = "module-generic-interface"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / Modules / StyioIR"
syntax = "No new source form; this is a versioned compiler artifact split behind existing import/export syntax."
resolution = "Publish a stable relation/effect/capability contract separately from a canonical typed StyioIR payload; consumers verify schema, semantic body, dependency, compiler, and target-independent digests before specialization."
golden_cases = [
  "tests/features/callable_interfaces/t01_downstream_specialization.styio",
  "tests/run_callable_interface_case.py",
]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/CallableInterface.cpp", "src/StyioSema/CallableModuleLoader.cpp", "src/StyioIR/PortableCallableBody.cpp", "src/StyioLowering/PortableCallableBody.cpp", "src/StyioIR/Verifier.cpp"]
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
path = "src/StyioIR/Verifier.cpp"
symbol = "verify_and_annotate_portable_callable_body"
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

Q7-A was approved on 2026-07-31. The public
scheme is the stable compatibility contract. A separate versioned payload
contains canonical typed StyioIR sufficient to reproduce a checked generic
body without reparsing defining source.

A consumer verifies the scheme, effect/capability facts, typed-body schema and
digest, direct dependency identities, target-independent semantic digest, and
compiler ABI before admitting the payload. The verifier rejects unknown nodes,
unbound symbols, mismatched types, stale dependencies, and noncanonical
serialization.

## Delivered Artifact Boundary

Callable interface schema v4 is a full cutover. Each entry carries a canonical
`styio.portable-styioir` schema-v1 payload and its target-independent semantic
digest; the module separately carries a public `contract_digest`, an aggregate
`typed_body_digest`, the direct-dependency digest, and the combined interface
ABI digest. There is no schema-v3 reader and no AST-text body field.

The payload is a bounded flat node table in child-before-parent order. Every
input must reference an earlier node, every non-root node has exactly one
parent, and the root has none. This makes validation linear in nodes plus
edges, rejects payload-local cycles structurally, and permits ownership-safe
AST reconstruction without recursive source parsing.

Portable StyioIR v1 admits parameter loads, scalar literals, numeric binary
operations, comparisons, Boolean logic, direct and concrete indirect calls,
nonempty uniform list/dictionary literals, indexed access, blocks, returns,
printing, final/mutable local bindings, and pass. Publication fails before
writing an interface when a checked body contains an instruction outside this
closed set.

The verifier rebuilds every node type from the stable callable signatures and
constraints. It also checks bound symbols, direct-call visibility, call arity,
constraint entailment, collection uniformity, encoded node types, root/result
agreement, schema/format, canonical JSON bytes, and semantic digests. Unknown
nodes and extra/noncanonical fields fail closed.

The loader reads dependency identities from the interface header, recursively
validates those interfaces, verifies the complete payload, and materializes
owned callable ASTs from it. The dependency source file is read only to bind
the source digest; it is never tokenized or parsed. The replay acceptance case
replaces that source with deliberately invalid Styio text, updates only its
source digest, and still executes the imported specializations.

Cross-module recursive SCCs remain rejected. A later whole-program module graph
must establish one owner and fixed point before that boundary can change.

## Diagnostic and Compatibility Boundary

Diagnostics distinguish stable scheme incompatibility from typed-body payload
invalidity. The payload is required, not optional. An invalid payload fails
closed; it never falls back to trusting source text, reparsing the defining
module, reading schema-v3 AST text, or executing opaque native code.

Non-generic exports retain their concrete ABI. Native generic ABI stability,
binary-only generic libraries, source-only reparsing, dynamic loading, and
cross-module mutual recursion are not approved here. Capturing environments
remain outside payload schema v1 and therefore cannot be published.
