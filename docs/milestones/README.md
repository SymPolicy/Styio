# Milestone Docs

**Purpose:** Define the scope and naming rules for active milestone batches under `docs/milestones/`; absorbed batches are removed from the current tree after their durable rules move into active docs, and the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-05-09

## Scope

1. Store only active or still-directly-relevant frozen milestone batches by date directory.
2. Keep active design drafts in `docs/plans/`.
3. Keep day-by-day execution notes in `docs/history/`.
4. Remove absorbed milestone batches from the current tree after durable rules move into active docs; use Git history for exact old wording.
5. If no current batch exists, keep only this README and the generated index.
6. Acceptance freezes still inherit the project-level priorities in [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Naming Rules

1. Batch directories use `YYYY-MM-DD/`.
2. Batch entry files use `00-Milestone-Index.md`.
3. Milestone files use `M<id>-<Topic>.md`.

## Inventory

See [INDEX.md](./INDEX.md).
