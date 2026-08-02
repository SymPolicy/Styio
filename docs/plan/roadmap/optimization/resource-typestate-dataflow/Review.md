assignment: agent=reviewer | role=reviewer | model=gpt-5.6-sol | reasoning_effort=max | basis=Intelligence Index | score=59 | benchmark=gpt-5-6-sol | source=codex-default-matrix

# OPT-F Review

**Purpose:** Record the sole repository-local OPT-F Reviewer outcome and bounded repairs.

**Last updated:** 2026-08-02

## Outcome

Accepted after bounded Reviewer repairs. The frozen file-handle scenario now has
one `CondFlowAST` may-closed union in Sema, SymbolId-authoritative state with the
string fallback restricted to invalid/uninterned IDs, branch-isolated close
transfers, missing-else identity, and reopen through the existing acquire/rebind
erase transfer. Runtime failure/effect families remain outside the typestate
lattice.

The review found no compatibility branch, legacy fall-through resource join,
parallel state owner, CFG/worklist/dominance/SCC solver, verifier inference, or
lowering/codegen reinterpretation for this slice. Match, loop, stream scheduling,
and OPT-G paths were not reviewed or changed.

## Reviewer repairs

1. Replaced misleading benchmark reuse of `alloc_count` and `ir_*` fields with
   dedicated resource-typestate snapshot, join, insertion, and peak temporary
   fact-slot fields. The benchmark continues to enforce the counters internally
   before emitting its sample.
2. Reset resource-typestate counters at each `MainBlockAST` analysis boundary so
   a reused Sema context starts a deterministic counter epoch.
3. Added an else-only close case to prove that the result is a real union rather
   than retained then-branch state, and added a reused-context counter lifecycle
   test.

## Changed paths

- `benchmark/internal/bench_utils.hpp`
- `benchmark/internal/core_bench.cpp`
- `src/StyioSema/TypeInfer.cpp`
- `tests/typeinfer_internal_test.cpp`
- `docs/plan/roadmap/optimization/resource-typestate-dataflow/Review.md`

## Focused evidence

- Impacted targets built successfully: `styio_typeinfer_internal_test`,
  `styio_security_test`, and `styio_core_bench`.
- The focused `StyioResourceTypestate.*` and
  `StyioSecurityResourceTypestate.*` selection passed all 13 tests.
- The 256-join benchmark completed and emitted truthful dedicated counters:
  512 branch snapshots, 256 joins, 256 fact insertions, and 766 peak temporary
  fact slots.
- Reviewer-touched paths passed `git diff --check`.

The group's complete frozen regression was deliberately not run by the Reviewer;
it remains the native main's single post-review final-validation action.

## Blockers

None.

## Decision issues

```json
[]
```
