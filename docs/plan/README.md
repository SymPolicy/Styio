# Plan Docs

**Purpose:** Define the unified `docs/plan/` workspace for active implementation plans, plan evidence reports, and Better Plan state; absorbed plans are removed from the current tree after durable rules move into active docs, and generated inventories live in [INDEX.md](./INDEX.md).

**Last updated:** 2026-06-28

## Scope

1. Store only active implementation plans, active migration plans, and still-open cross-repo contracts here.
2. These files are not language or acceptance SSOT.
3. A plan may remain here after a repo-local baseline closes only if it still governs hardening, cross-repository alignment, or later closure work; when that happens, the file must state its current status explicitly near the top.
4. When a plan is superseded or its durable knowledge has been absorbed into active docs, remove it from the current tree and rely on Git history for exact old wording.
5. Store plan evidence reports under [reports/](./reports/) when the evidence still supports active rollups, ADR follow-up, or pending acceptance work.
6. Store Better Plan state at [Manifest.json](./Manifest.json) and each plan directory's `Checkpoints.json`; there is no separate `better-plan` directory.
7. Tradeoff order for plans still follows [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Required Plan Shape

Every plan document in this directory, except this README and the generated INDEX, must include these exact H2 sections:

1. `## 前置条件`
2. `## 验收条件`

`## 前置条件` must explicitly say whether work can run in parallel, whether sub-agents may be started, and whether any common foundation work must land first. Plans should default to parallel-first execution: split read-only inventory, evidence gathering, downstream confirmation, and test discovery into independent lanes; keep only shared foundation changes, semantic decisions, public contract shape changes, and final SSOT updates behind a named serial merge gate. Custom planning sections may appear between the two required sections.

`## 验收条件` must name the acceptance evidence, owner gates, or exit criteria that prove the plan is complete.

## 通用基座计划索引

Common foundation work must be routed through exactly one foundation plan per foundation key. A feature or application-layer plan that discovers foundation work must put the foundation checkpoint first, then split upper-layer work only after the foundation plan's prerequisites and acceptance conditions are satisfied.

| Foundation key | Owning plan | Scope |
|----------------|-------------|-------|
| `common-foundation` | [Styio-Common-Foundation-Plan.md](./Styio-Common-Foundation-Plan.md) | Shared workflow gates, repo-local skills, docs/process gates, test harness entrypoints, compiler service contracts, and other substrate changes that unblock multiple upper-layer feature plans. |

## Status Rules

1. Use explicit top-level status wording such as `Active`, `Repo-local baseline completed`, or `Completed and ready for deletion after promotion`.
2. If a repo-local baseline is complete but ecosystem closure remains open elsewhere, say so directly and link the owning master plan.
3. If a plan is still the sequencing document for unfinished work, keep it short and link the owning SSOT instead of duplicating details.
4. `docs/plan/INDEX.md`, `docs/plan/reports/INDEX.md`, and repository entry docs should be able to answer "is this still active?" without forcing readers to infer it from stage tables.

## Naming Rules

1. Use descriptive names such as `<Topic>-Plan.md`, `<Topic>-Implementation-Plan.md`, or `<Topic>-Adjustment.md`.
2. Do not add generic filenames such as `idea.md`, `notes.md`, or `misc.md`.
3. Historical plans should keep their filenames stable once they are referenced elsewhere.

## Inventory

1. Generated plan inventory: [INDEX.md](./INDEX.md)
2. Evidence report inventory: [reports/INDEX.md](./reports/INDEX.md)
3. Better Plan manifest: [Manifest.json](./Manifest.json)
