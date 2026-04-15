# Styio Current State

**Purpose:** Provide the compressed default read-in for the current repository state so future agents can orient themselves before loading raw history or archive docs.

**Last updated:** 2026-04-15

## Default Read Order

1. Project priorities: [`../specs/PRINCIPLES-AND-OBJECTIVES.md`](../specs/PRINCIPLES-AND-OBJECTIVES.md)
2. Current state: this file
3. Active SSOT by topic:
   - language/runtime semantics: [`../design/INDEX.md`](../design/INDEX.md)
   - project/doc rules: [`../specs/INDEX.md`](../specs/INDEX.md)
   - open design contradictions: [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md)
   - workflow and gates: [`../assets/workflow/INDEX.md`](../assets/workflow/INDEX.md)
4. Current implementation front: [`../milestones/2026-04-15/00-Milestone-Index.md`](../milestones/2026-04-15/00-Milestone-Index.md) and [`../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)
5. Recent raw execution log: newest file in [`../history/INDEX.md`](../history/INDEX.md)
6. Exact historical wording: [`../archive/ARCHIVE-LEDGER.md`](../archive/ARCHIVE-LEDGER.md)

## Stable Baseline

1. Project-level decision order is fixed by [`../specs/PRINCIPLES-AND-OBJECTIVES.md`](../specs/PRINCIPLES-AND-OBJECTIVES.md): performance first, usability second, and valuable rewrites are allowed even when compatibility breaks.
2. Language/runtime acceptance is frozen through M1-M10. For standard streams, canonical writes are `expr -> @stdout/@stderr`, `expr >> @stdout/@stderr` remains accepted compatibility shorthand, and `(<< @stdin)` currently stays on the numeric instant-pull contract.
3. The active parser/toolchain baseline is nightly-first rather than legacy-first. Shadow zero-fallback and five-layer pipeline coverage are part of the normal correctness story, not optional side tests.
4. Repository docs now distinguish active summaries (`docs/rollups/`), active raw windows (`docs/history/`, `docs/review/`), and archived provenance (`docs/archive/`).

## Current Development Front

1. The active roadmap is the IDE batch M11-M19 in [`../milestones/2026-04-15/`](../milestones/2026-04-15/). It focuses on incremental edits, semantic query caching, stable HIR, resolver-backed semantics, completion quality, workspace indexing, runtime scheduling, and latency/perf closure.
2. The corresponding execution sequence lives in [`../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md). Treat the milestone batch as frozen acceptance and the plan as sequencing detail.
3. Resource Topology v2 remains a target design rather than the running canonical syntax. Current implementation still keeps the M6 path canonical until a dedicated migration milestone lands.

## Active Gates

1. Docs structure: `python3 scripts/docs-index.py --write`, `python3 scripts/docs-audit.py`, `python3 scripts/docs-lifecycle.py validate`
2. Core correctness: `ctest --test-dir build -L milestone`, `ctest --test-dir build -L styio_pipeline`, `ctest --test-dir build -L security`
3. Parser migration health: shadow gate / zero-fallback / zero-internal-bridges flows described in [`../assets/workflow/TEST-CATALOG.md`](../assets/workflow/TEST-CATALOG.md)
4. Benchmark/perf workflow: `benchmark/perf-route.sh` plus structured local reports; compare `results.json` / `benchmarks.csv`, not terminal screenshots

## Current Risks

1. [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md) still records unresolved syntax and semantic overloading around `<<`, `>>`, `@`, `&`, string coercion, and state lifetime.
2. The IDE batch is specified but not fully closed; stable semantic identity, fine-grained caches, and runtime scheduling discipline are still active work.
3. Benchmarking is now structured, but meaningful comparisons still depend on keeping parser shadow/five-layer gates green alongside the perf route.
