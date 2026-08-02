assignment: agent=reviewer | role=reviewer | model=gpt-5.6-sol | reasoning_effort=max | basis=Intelligence Index | score=59 | benchmark=gpt-5-6-sol | source=codex-default-matrix

# OPT-E Review

**Purpose:** Record the sole repository-local OPT-E Reviewer outcome and bounded repairs.

**Last updated:** 2026-08-02

## Outcome

Within the reviewed repository-local boundary and evidence, OPT-E is ready for
the state owner's one normal full impacted regression. Both callable-constraint
phases use the same dependency-indexed runner; substitution and binding-fact
ownership remain unchanged; origin-ordered diagnostics, numeric-only defaulting,
SCC relations, richer concrete facts, and the frozen structural budgets are
covered by executable evidence. This record does not establish repository-wide,
release, OPT-F, or OPT-G completion.

## End-to-end scope reviewed

- all three OPT-E Nodes, `Architecture.md`, `Validation.md`, and the recorded
  implementation handoff;
- `src/StyioSema/SemaContext.hpp` and `src/StyioSema/TypeInfer.cpp`, including
  symbolic reduction, concrete instantiation, statistics reset/aggregation,
  canonical scheme construction, and specialization consumers;
- `tests/CMakeLists.txt`, `tests/typeinfer_internal_test.cpp`, and the existing
  `callable_constraints` and `inferred_generics` positive/negative fixtures;
- the OPT-E sample in `benchmark/internal/core_bench.cpp` and its JSON evidence
  seam.

OPT-F/OPT-G, parser, IR optimization, code generation, runtime, and unrelated
benchmark samples were not audited.

## Repairs

1. Replaced binding-delta duplicate suppression's linear search with one
   intrusive `V`-slot scratch list. Append and membership are now O(1), drain
   visits only changed variables, duplicate wakeups remain impossible, and the
   frozen `3C + 2V` scheduler-storage budget is unchanged.
2. Replaced concrete default-eligibility preprocessing that repeatedly collected
   variables and rescanned each subject with direct linear term marking. The
   numeric-only rule is unchanged, but default preparation is now proportional
   to the visited constraint-term structure rather than quadratic in variables
   per constraint.
3. Strengthened the realistic benchmark oracle so the sample proves one actual
   256-constraint frontier: frontier, blocked, and live-waiter peaks must each be
   exactly 256, in addition to the existing attempt and storage budgets. The
   observed aggregate input remains 257 because canonical scheme deduplication
   intentionally reduces the concrete phase to one obligation.
4. Added the required top-level `Purpose` and `Last updated` metadata to all
   three OPT-E documents and kept the outcome explicitly repository-local and
   evidence-scoped.

The recorded pattern rejection remains correct. The implementation retains one
module-local runner and direct symbolic/concrete clients; no strategy hierarchy,
service layer, compatibility solver, or runtime algorithm selection was added.

## Bounded verification

- `styio_typeinfer_internal_test` and `styio_core_bench` targets built
  successfully. Existing unrelated override warnings remained outside OPT-E.
- All seven `StyioSemaCallableConstraint.*` cases passed after the repairs.
- All 18 `callable_constraints` and `inferred_generics` feature and diagnostic
  cases passed after the repairs.
- The bounded benchmark smoke completed with the
  `constraint/callable_worklist_fanout_256` sample and 772 reported scheduler
  slots, matching `3 * 256 + 2 * 2`.
- The one-time migration audit confirmed both removed whole-set scan bodies are
  absent and found exactly one common runner definition with symbolic and
  concrete uses.
- Targeted metadata and evidence-wording checks passed for all three OPT-E
  documents. The repository-wide documentation audit then stopped on an
  OPT-D-owned evidence-wording failure outside this review boundary.
- The frozen group full regression was intentionally not run by the Reviewer;
  it remains the state owner's next and only normal full-regression action.

## Reviewer-changed repository-relative paths

- `src/StyioSema/TypeInfer.cpp`
- `tests/typeinfer_internal_test.cpp`
- `benchmark/internal/core_bench.cpp`
- `docs/plan/roadmap/optimization/bounded-type-constraints/Architecture.md`
- `docs/plan/roadmap/optimization/bounded-type-constraints/Validation.md`
- `docs/plan/roadmap/optimization/bounded-type-constraints/Review.md`

## Blockers

None within OPT-E. The shared repository documentation gate currently also
depends on an OPT-D-owned wording repair; the state owner must coordinate that
independent correction before expecting the repository-wide gate to pass.

## Decision issues

```json
[]
```
