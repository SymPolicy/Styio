# History Docs

**Purpose:** Define the scope and recovery usage of `docs/history/`; the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-04-15

## Scope

1. Keep only the recent raw daily implementation history and recovery notes here.
2. Fold durable lessons into `../rollups/` and archive older raw history in `../archive/history/`.
3. Keep durable workflow rules in `docs/assets/workflow/`.
4. Keep decisions that must survive recovery in `docs/adr/`.
5. History records execution details; project-level priority order still comes from [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Recovery Order

1. Read [../rollups/CURRENT-STATE.md](../rollups/CURRENT-STATE.md).
2. Read the newest raw date file in [INDEX.md](./INDEX.md).
3. Check [../assets/workflow/CHECKPOINT-WORKFLOW.md](../assets/workflow/CHECKPOINT-WORKFLOW.md) for the current recovery gate.
4. Jump to [../adr/INDEX.md](../adr/INDEX.md) when a checkpoint depends on a prior decision.
5. Use [../archive/ARCHIVE-LEDGER.md](../archive/ARCHIVE-LEDGER.md) only when you need older provenance.

## Inventory

See [INDEX.md](./INDEX.md).
