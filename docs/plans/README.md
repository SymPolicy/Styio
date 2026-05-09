# Plans Docs

**Purpose:** Define the scope and naming rules for the small set of active implementation plans under `docs/plans/`; absorbed plans are removed from the current tree after durable rules move into active docs, and the generated file inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-05-09

## Scope

1. Store only active implementation plans, active migration plans, and still-open cross-repo contracts here.
2. These files are not language or acceptance SSOT.
3. A plan may remain here after a repo-local baseline closes only if it still governs hardening, cross-repository alignment, or later closure work; when that happens, the file must state its current status explicitly near the top.
4. When a plan is superseded or its durable knowledge has been absorbed into active docs, remove it from the current tree and rely on Git history for exact old wording.
5. Tradeoff order for plans still follows [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Status Rules

1. Use explicit top-level status wording such as `Active`, `Repo-local baseline completed`, or `Completed and ready for deletion after promotion`.
2. If a repo-local baseline is complete but ecosystem closure remains open elsewhere, say so directly and link the owning master plan.
3. If a plan is still the sequencing document for unfinished work, keep it short and link the owning SSOT instead of duplicating details.
4. `docs/plans/INDEX.md` and repository entry docs should be able to answer "is this still active?" without forcing readers to infer it from stage tables.

## Naming Rules

1. Use descriptive names such as `<Topic>-Plan.md`, `<Topic>-Implementation-Plan.md`, or `<Topic>-Adjustment.md`.
2. Do not add generic filenames such as `idea.md`, `notes.md`, or `misc.md`.
3. Historical plans should keep their filenames stable once they are referenced elsewhere.

## Inventory

See [INDEX.md](./INDEX.md).
