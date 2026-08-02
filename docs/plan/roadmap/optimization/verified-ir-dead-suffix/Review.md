assignment: agent=reviewer | role=reviewer | model=gpt-5.6-sol | reasoning_effort=max | basis=Intelligence Index | score=59 | benchmark=gpt-5-6-sol | source=codex-default-matrix

# OPT-D Review

**Purpose:** Record the sole repository-local OPT-D Reviewer outcome and bounded repairs.

**Last updated:** 2026-08-02

## Outcome

Accepted after bounded Reviewer repairs. The pass remains a direct, single-walk,
verifier-guarded transform with deterministic counters, stable retained-node order,
no pass-owned heap allocation, and no compatibility or former no-op route.

## End-to-end scope reviewed

- all three OPT-D Nodes plus `Architecture.md` and `Validation.md`;
- `src/StyioLowering/StyioIROptimizer.hpp` and
  `src/StyioLowering/StyioIROptimizer.cpp`;
- the actually impacted `StyioIRWalker`, verifier, lowering-fragment, pass-manager,
  and `SGMainEntry` codegen paths;
- `tests/lowering_internal_test.cpp`,
  `tests/security/styio_security_test.cpp`, and
  `benchmark/internal/core_bench.cpp`.

Parser, Sema, unrelated optimizer families, runtime branches, and other known
untouched capabilities were not audited.

## Repairs

1. Preserved `SGMainEntry` nodes consumed by codegen's predeclaration and capture
   scan (`SGFunc`, export/extern declarations, and top-level bindings). Runtime-dead
   siblings are still deleted through stable in-place compaction, while retained
   compile-time-live nodes and nested bodies remain traversed. This closes a real
   forward-declaration semantic-equivalence hole without a CFG, worklist, clone, or
   extra allocation.
2. Prevented dead-suffix elimination from running while loop-control legality is
   explicitly deferred for an intermediate `SGBlock`. The default intermediate
   pipeline keeps canonicalization and constant folding, preserves the fragment,
   and lets the strict final-root pipeline perform OPT-D after legality is known.
   Explicit manual insertion with deferred legality now fails closed before mutation.
3. Tightened the verifier's owning-tree contract: a reused pointer or cycle is now
   rejected before the pass can delete an aliased suffix and leave a dangling live
   owner or double-destroy a node. The general verifier retains its DAG-alias
   compatibility; only mutating pass boundaries opt into unique ownership.
4. Strengthened focused oracles for exact record counters, zero statistics on other
   passes, predeclaration preservation through real JIT execution, deferred-verifier
   ordering, and alias rejection.

The recorded pattern rejection remains correct. No catalog pattern was introduced;
the implementation uses the existing walker, one local classification predicate,
and one stable vector compaction loop.

## Bounded verification

- `styio_security_test` and `styio_core_bench` targets built successfully.
- Ten exact OPT-D pass-manager and security/JIT cases passed.
- The frozen group full regression was intentionally not run by the Reviewer; it
  remains the state owner's next and only normal full-regression action.

## Reconciliation note

`OPT-D-RECONCILE` corrected the canonical `design.symbols` entry to
`StyioIRPassStatistics run_dead_stmt_elim_pass(StyioIR*)` and synchronized
Architecture and Validation with the Reviewer-owned predeclaration, deferred
loop-control, and unique-ownership repairs. No compatibility signature or second
implementation path was added.

## Blockers

None.

## Decision issues

```json
[]
```
