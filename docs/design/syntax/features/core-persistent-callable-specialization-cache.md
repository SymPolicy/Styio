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
delivery_state = "converged"
owner = "Codegen / Compiler Infrastructure"
syntax = "No source form; cache location and retention are compiler operational configuration and cannot change program meaning."
resolution = "Reuse verified native specialization artifacts by the existing full content digest in a compiler/target-namespaced local cache with atomic writes, corruption fallback, measured lookup cost, and explicit age/size/file-count pruning."
golden_cases = ["tests/features/persistent_callable_cache/t01_native_specializations.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/CallableSpecializationGraph.cpp", "src/StyioCodeGen/CallableSpecializationObjectCache.cpp", "src/StyioCodeGen/CodeGenG.cpp", "src/StyioJIT/StyioJIT_ORC.hpp", "src/main.cpp"]
evidence = ["tests/features/persistent_callable_cache/t01_native_specializations.styio", "tests/run_persistent_callable_cache_case.py"]

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
path = "src/StyioCodeGen/CallableSpecializationObjectCache.cpp"
symbol = "CallableSpecializationObjectCache::getObject"
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

Q8-A was approved on 2026-07-31 and is now converged. Reuse is local and
content-addressed by the full specialization digest. Cache namespaces include
compiler ABI, LLVM version, native-codegen schema, target, pointer width,
edition/channel, and backend facts. Writes are atomic; read corruption or
verification failure discards the entry and recompiles.

Retention has explicit age, byte-size, and file-count ceilings with
deterministic pruning. Hash, lookup, verification, and materialization costs
must be measured so cache enablement cannot silently regress clean builds.

## Implemented Native-Artifact Boundary

The cache is opt-in through `--callable-cache-dir`. With no cache root, codegen
keeps the original single-module ORC path and performs no specialization
partitioning, cache hashing, directory scan, or disk write.

When enabled, every reachable `SGFunc` specialization carries its existing
64-hex content digest into LLVM as a function attribute. Before ORC
materialization, codegen deterministically sorts those functions by digest,
clones each definition into one independent LLVM module, converts the main
module's copy to a declaration, and verifies both the partitions and the
remaining main module. A specialization partition owns its function plus
module-local constants; calls to other concrete specializations and runtime
helpers stay external. ORC's `ConcurrentIRCompiler` queries
`CallableSpecializationObjectCache` before native compilation, so a hit
materializes the verified object directly and a miss compiles only that
partition.

The cache file key is the current specialization digest, which Sema recomputes
from the canonical concrete relation, canonical effects and usage facts,
portable-body digest, transitive callable dependency digest, direct module
dependency/interface digest, and the active backend ABI. Comparing that
freshly computed key to the entry therefore revalidates transitive semantic
dependencies before reuse. The namespace is a SHA-256 digest of the
full/nano compiler channel, compiler version, edition, target/pointer facts,
dictionary backend, LLVM version, and native-codegen schema. Cache placement
and process addresses are never identity inputs.

Each binary entry has one schema-v1 fixed header containing the specialization
digest, namespace digest, expected native symbol, object length, and object
SHA-256, followed by the object bytes. Reads are bounded, reject symlinks and
unsafe file permissions on hosts that expose POSIX ownership, validate the
header and checksum, parse the bytes with LLVM's object reader, verify target
architecture, and require the expected defined symbol. The verified bytes are
then copied into an ORC-owned buffer. A malformed, truncated, stale,
wrong-target, wrong-symbol, or otherwise unverifiable entry is removed and
treated as one cache miss.

Writes use a unique file in the namespace directory, owner-only permissions,
and same-directory rename. Concurrent writers for an equal digest converge on
one complete entry; no reader observes a partial destination.

## Retention and Measurement

The operational controls are:

```text
--callable-cache-dir=<root>
--callable-cache-max-age-seconds=<positive integer>  # default 604800
--callable-cache-max-bytes=<positive integer>        # default 268435456
--callable-cache-max-files=<positive integer>        # default 4096
--callable-cache-stats
```

Pruning scans only the active hashed namespace and only canonical
`<sha256>.styobj` regular files. Candidates are sorted by modification time
and then filename. Expired entries are removed first; the same oldest-first
order enforces byte and file ceilings. The algorithm is `O(n log n)` for `n`
entries. It performs one scan on first lookup, maintains retained byte/file
counters for ordinary writes, rescans immediately when a projected ceiling is
crossed, and performs a reconciliation scan every 64 successful writes so
concurrent-process drift is bounded without making a cold build rescan the
directory once per specialization.

Explicit statistics mode writes one path-free
`styio.callable-cache-stats.v1` JSON object to stderr. It reports
lookups/hits/misses/corruptions/writes/evictions/I/O failures and nanoseconds
spent hashing, looking up, verifying, and materializing. Normal compiler output
contains no cache diagnostics or machine-specific cache path.

## Security, Diagnostic, and Compatibility Boundary

The local cache is untrusted optimization state. A malformed entry is a cache
miss, not a language error. Cache directory or entry I/O failure also degrades
to native recompilation; only invalid CLI configuration is a CLI error.
Diagnostics may report cache statistics in the explicit mode but must not
expose machine-specific paths.

Distributed caches, signatures, provenance, remote trust, stable process
addresses, dynamic loading, link-unit ownership, and profile-guided semantic
selection require separate decisions.
