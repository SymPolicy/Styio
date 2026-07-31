# Persistent Callable Specialization Cache

**Purpose:** Own bounded cross-invocation reuse of verified concrete callable specializations while keeping semantic callable identity independent of process addresses and cache placement.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.persistent-callable-specialization-cache"
title = "Persistent Callable Specialization Cache"
kind = "compiler-specialization-cache"
decision_state = "accepted"
delivery_state = "not_started"
owner = "Codegen / Compiler Infrastructure"
syntax = "No source form; cache location and retention are compiler operational configuration and cannot change program meaning."
resolution = "Reuse verified native specialization artifacts by the existing full content digest in a compiler/target-namespaced local cache with atomic writes, corruption fallback, measured lookup cost, and explicit age/size/file-count pruning."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/CallableSpecializationGraph.cpp", "src/StyioCodeGen/CodeGenG.cpp"]
evidence = ["tests/features/callable_specialization/t01_reachable_instances.styio"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_specialization/t01_reachable_instances.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
cache-identity-contract = "docs/design/syntax/features/core-callable-specialization-policy.md"
portable-body-contract = "docs/design/syntax/features/core-portable-generic-body-interface.md"

[implementation]
owner = "Codegen / Compiler Infrastructure"

[dependencies]
requires = [
  { id = "core.callable-specialization-policy", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.monomorphic-callable-values", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.portable-generic-body-interface", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.callable-specialization-policy"]
conflicts = []
supersedes = []
after = ["core.portable-generic-body-interface"]
```

## Decision

Q8-A was approved as a blocked direction on 2026-07-31. Reuse is local and
content-addressed by the full specialization digest. Cache namespaces include
compiler ABI, target, edition/channel, and backend facts. Writes are atomic;
read corruption or verification failure discards the entry and recompiles.

Retention has explicit age, byte-size, and file-count ceilings with
deterministic pruning. Hash, lookup, verification, and materialization costs
must be measured so cache enablement cannot silently regress clean builds.

## Blocked Delivery Boundary

Delivery waits for the portable verified typed-body payload and monomorphic
callable identity. Every reused artifact must revalidate its transitive
dependency digest. Cache absence, eviction, or corruption cannot change
program behavior.

## Security, Diagnostic, and Compatibility Boundary

The local cache is untrusted optimization state. A malformed entry is a cache
miss, not a language error. Diagnostics may report cache statistics in an
explicit diagnostic mode but must not expose machine-specific paths in normal
compiler output.

Distributed caches, signatures, provenance, remote trust, stable process
addresses, dynamic loading, link-unit ownership, and profile-guided semantic
selection require separate decisions.
