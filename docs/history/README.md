# History Docs

**Purpose:** Define the scope and recovery usage of `docs/history/`; the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-04-08

## Scope

1. Store daily implementation history and recovery notes here.
2. Keep durable workflow rules in `docs/assets/workflow/`.
3. Keep decisions that must survive recovery in `docs/adr/`.

## Recovery Order

1. Read the newest date file in [INDEX.md](./INDEX.md).
2. Check [../assets/workflow/CHECKPOINT-WORKFLOW.md](../assets/workflow/CHECKPOINT-WORKFLOW.md) for the current recovery gate.
3. Jump to [../adr/INDEX.md](../adr/INDEX.md) when a checkpoint depends on a prior decision.

## Inventory

See [INDEX.md](./INDEX.md).
