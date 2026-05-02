# Styio Current State

**Purpose:** Provide the compressed default read-in for the current repository state so future agents can orient themselves from active docs first; raw history, archived milestones/plans, and provenance docs are optional background, not required maintenance input.

**Last updated:** 2026-04-24

## Default Read Order

1. Project priorities: [`../specs/PRINCIPLES-AND-OBJECTIVES.md`](../specs/PRINCIPLES-AND-OBJECTIVES.md)
2. Current state: this file
3. Active next-stage gap ledger: [`./NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md)
4. Active next-stage checkpoint tree: [`../plans/Next-Stage-Checkpoint-Tree.md`](../plans/Next-Stage-Checkpoint-Tree.md)
5. Active SSOT by topic:
   - language/runtime semantics: [`../design/INDEX.md`](../design/INDEX.md)
   - project/doc rules: [`../specs/INDEX.md`](../specs/INDEX.md)
   - open design contradictions: [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md)
   - workflow and gates: [`../assets/workflow/INDEX.md`](../assets/workflow/INDEX.md)
6. Current implementation front: [`../milestones/2026-04-15/00-Milestone-Index.md`](../milestones/2026-04-15/00-Milestone-Index.md) and [`../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)
7. Optional provenance only when active docs are insufficient: [`../archive/rollups/HISTORICAL-LESSONS.md`](../archive/rollups/HISTORICAL-LESSONS.md), newest raw entry in [`../history/INDEX.md`](../history/INDEX.md), and [`../archive/ARCHIVE-LEDGER.md`](../archive/ARCHIVE-LEDGER.md)

## Stable Baseline

1. Project-level decision order is fixed by [`../specs/PRINCIPLES-AND-OBJECTIVES.md`](../specs/PRINCIPLES-AND-OBJECTIVES.md): performance first, usability second, and valuable rewrites are allowed even when compatibility breaks.
2. Language/runtime acceptance is frozen through M1-M10. For standard streams, scalar writes are `expr -> @stdout/@stderr`, iterable writes are `items >> @stdout/@stderr`, stdin line iteration is `@stdin >> #(line) => {...}`, and immediate pull is `(<- @stdin)`. Compatibility forms remain accepted where already frozen: terminal spelling `(>_)` / symbolic stdin `<|(>_)` and legacy `(<< @stdin)` on the numeric instant-pull contract.
3. The active parser/toolchain baseline is nightly-first rather than legacy-first. Shadow zero-fallback and five-layer pipeline coverage are part of the normal correctness story, not optional side tests.
4. Repository docs now distinguish active maintenance docs (`design/specs/teams/assets/workflow/current rollups`), optional raw windows (`docs/history/`, `docs/review/`), and archived provenance (`docs/archive/`).
5. File governance is now on the shared three-repo baseline: root `.gitignore` freezes the common ignore floor, `docs/**` and `tests/**` temp/build-style tracked fixtures use explicit negate rules, and `scripts/repo-hygiene-gate.py` checks both the patterns and the key governance doc links.

## Current Development Front

1. The active roadmap is the IDE batch M11-M19 in [`../milestones/2026-04-15/`](../milestones/2026-04-15/). It focuses on incremental edits, semantic query caching, stable HIR, resolver-backed semantics, completion quality, workspace indexing, runtime scheduling, and latency/perf closure.
2. The corresponding execution sequence lives in [`../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md). Treat the milestone batch as frozen acceptance and the plan as sequencing detail.
3. The active repo-wide unfinished-work summary for next-stage planning is [`./NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md). Use it to split compiler debt, IDE closure work, and `spio` handoff tasks without collapsing repo boundaries.
4. Resource Topology v2 remains a target design rather than the running canonical syntax. Current implementation still keeps the M6 path canonical until a dedicated migration milestone lands.

## Active Gates

1. File/docs governance floor: `python3 scripts/repo-hygiene-gate.py --mode tracked`, `python3 scripts/team-docs-gate.py`, `python3 scripts/docs-index.py --write`, `python3 scripts/docs-audit.py`, `python3 scripts/docs-lifecycle.py validate`
2. Common delivery floor: `./scripts/delivery-gate.sh` (default safe auto mode; pass `--base <ref>` only when the intended branch/promotion base cannot be inferred)
3. Core correctness: `ctest --test-dir build -L milestone`, `ctest --test-dir build -L styio_pipeline`, `ctest --test-dir build -L security`
4. Parser migration health: shadow gate / zero-fallback / zero-internal-bridges flows described in [`../assets/workflow/TEST-CATALOG.md`](../assets/workflow/TEST-CATALOG.md)
5. Benchmark/perf workflow: `benchmark/perf-route.sh` plus structured local reports; compare `results.json` / `benchmarks.csv`, not terminal screenshots

## Optional Provenance

1. `docs/history/` and `docs/archive/` exist for recovery, audit, and exact wording traceability. They are not required for normal maintenance when active docs are sufficient.
2. `docs/archive/milestones/`, `docs/archive/plans/`, and `docs/archive/rollups/` hold absorbed acceptance snapshots, historical implementation plans, and compressed provenance digests after their durable rules have been lifted into active docs.

## Current Risks

1. [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md) still records unresolved syntax and semantic overloading around `<<`, `>>`, `@`, `&`, string coercion, and state lifetime.
2. The deepest remaining implementation debt is now summarized in [`./NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md): parser subset gaps, sema/lowering placeholders, incomplete M7 stream closure, compile-plan release hardening with `styio-spio`, and IDE stdio runtime drain.
3. The IDE batch is specified but not fully closed; stable semantic identity, fine-grained caches, and runtime scheduling discipline are still active work.
4. Benchmarking is now structured, but meaningful comparisons still depend on keeping parser shadow/five-layer gates green alongside the perf route.
5. Shared ignore/fixture governance is only frozen for current tracked roots; any future repro root outside `docs/**` or `tests/**` still needs explicit negate rules before files land.
