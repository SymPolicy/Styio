# Styio Current State

**Purpose:** Provide the compressed default read-in for the current repository state so future agents can orient themselves from active docs first; Git history and minimal lifecycle provenance are optional background, not required maintenance input.

**Last updated:** 2026-05-09

## Default Read Order

1. Project priorities: [`../specs/PRINCIPLES-AND-OBJECTIVES.md`](../specs/PRINCIPLES-AND-OBJECTIVES.md)
2. Current state: this file
3. Active next-stage gap ledger: [`./NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md)
4. Active SSOT by topic:
   - language/runtime semantics: [`../design/INDEX.md`](../design/INDEX.md)
   - project/doc rules: [`../specs/INDEX.md`](../specs/INDEX.md)
   - workflow and gates: [`../assets/workflow/INDEX.md`](../assets/workflow/INDEX.md)
   - team execution: [`../teams/INDEX.md`](../teams/INDEX.md)
5. Current implementation front: [`./NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md), plus the owning team runbooks for each touched area
6. Optional provenance only when active docs are insufficient: Git history for exact old wording, [`../adr/IMPLEMENTED-DECISIONS.md`](../adr/IMPLEMENTED-DECISIONS.md) for compressed decisions, and [`../archive/ARCHIVE-LEDGER.md`](../archive/ARCHIVE-LEDGER.md) for lifecycle metadata

## Stable Baseline

1. Project-level decision order is fixed by [`../specs/PRINCIPLES-AND-OBJECTIVES.md`](../specs/PRINCIPLES-AND-OBJECTIVES.md): performance first, usability second, and valuable rewrites are allowed even when compatibility breaks.
2. Language/runtime acceptance is frozen through language feature acceptance suites. For standard streams, scalar writes are `expr -> @stdout/@stderr`, iterable writes are `items >> @stdout/@stderr`, stdin line iteration is `@stdin >> #(line) => {...}`, and immediate pull is `(<- @stdin)`. Compatibility forms remain accepted where already frozen: terminal spelling `(>_)` / symbolic stdin `<|(>_)` and legacy `(<< @stdin)` on the numeric instant-pull contract.
3. The active parser/toolchain baseline is nightly-first rather than legacy-first. Shadow zero-fallback and five-layer pipeline coverage are part of the normal correctness story, not optional side tests.
4. Repository docs now distinguish active maintenance docs (`design/specs/teams/assets/workflow/current rollups`) from Git-history provenance and the minimal `docs/archive/` lifecycle shell.
5. File governance is now on the shared three-repo baseline: root `.gitignore` freezes the common ignore floor, `docs/**` and `tests/**` temp/build-style tracked fixtures use explicit negate rules, and `scripts/repo-hygiene-gate.py` checks both the patterns and the key governance doc links.

## Current Development Front

1. The active repo-wide unfinished-work summary is [`./NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md). Use it to split compiler debt, IDE closure work, and `spio` handoff tasks without collapsing repo boundaries.
2. IDE work is tracked through the IDE external docs, IDE/LSP runbook, perf runbook, and current gap ledger instead of a retained planning batch.
3. Resource Topology v2 remains a dedicated migration track owned by [`../design/Styio-Resource-Topology.md`](../design/Styio-Resource-Topology.md), [`../design/syntax/ACTIVE-SYNTAX.md`](../design/syntax/ACTIVE-SYNTAX.md), and the current gap ledger.

## Active Gates

1. File/docs governance floor: `python3 scripts/repo-hygiene-gate.py --mode tracked`, `python3 scripts/team-docs-gate.py`, `python3 scripts/docs-index.py --write`, `python3 scripts/docs-audit.py`, `python3 scripts/docs-lifecycle.py validate`
2. Common delivery floor: `./scripts/delivery-gate.sh` (default safe auto mode; pass `--base <ref>` only when the intended branch/promotion base cannot be inferred)
3. Core correctness: `ctest --test-dir build -L language_feature`, `ctest --test-dir build -L styio_pipeline`, `ctest --test-dir build -L security`
4. Parser migration health: shadow gate / zero-fallback / zero-internal-bridges flows described in [`../assets/workflow/TEST-CATALOG.md`](../assets/workflow/TEST-CATALOG.md)
5. Benchmark/perf workflow: `styio-benchmark/tools/perf-route.sh --styio-root <styio-checkout>` plus structured reports in `styio-benchmark`; Styio keeps only probes and compatibility wrappers

## Optional Provenance

1. `docs/history/`, `docs/review/`, and `docs/adr/` keep only active entrypoints or compressed summaries; exact old wording is recovered from Git history when needed.
2. `docs/archive/` is a lifecycle metadata shell, not a place to keep old syntax catalogs, archived examples, archived source snapshots, old plans, old rollups, or absorbed planning batches in the current tree.

## Current Risks

1. The deepest remaining implementation debt is summarized in [`./NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md): parser subset gaps, sema/lowering placeholders, incomplete stream-processing stream closure, compile-plan release hardening with `styio-spio`, and IDE operational closure.
2. Resource topology migration must keep design, parser/sema/lowering work, and test catalog updates in one checkpoint path.
3. Benchmarking is now structured, but meaningful comparisons still depend on keeping parser shadow/five-layer gates green alongside the perf route.
4. Shared ignore/fixture governance is only frozen for current tracked roots; any future repro root outside `docs/**` or `tests/**` still needs explicit negate rules before files land.
