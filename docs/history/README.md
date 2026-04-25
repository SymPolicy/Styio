# History Docs

**Purpose:** Define the scope and recovery usage of `docs/history/`; these files store raw checkpoint and execution traces, not the active maintenance truth.

**Last updated:** 2026-04-15

## Scope

1. Keep only the recent raw daily implementation history and recovery notes here.
2. Fold durable lessons into `../rollups/` and archive older raw history in `../archive/history/`.
3. Keep durable workflow rules in `docs/assets/workflow/`.
4. Keep current durable rules in active SSOTs and runbooks; keep decision provenance in `docs/adr/` only until it has been absorbed or archived.
5. History records execution details; project-level priority order still comes from [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Recovery Order

1. Read [../rollups/CURRENT-STATE.md](../rollups/CURRENT-STATE.md).
2. Read the owning `design/specs/teams/assets/workflow` documents for the topic you are touching.
3. Check [../assets/workflow/CHECKPOINT-WORKFLOW.md](../assets/workflow/CHECKPOINT-WORKFLOW.md) for the current recovery gate.
4. Read the newest raw date file in [INDEX.md](./INDEX.md) only when an interrupted checkpoint still needs exact local trace details.
5. Jump to [../adr/INDEX.md](../adr/INDEX.md) or [../archive/ARCHIVE-LEDGER.md](../archive/ARCHIVE-LEDGER.md) only when current docs do not explain the surviving rule or when exact historical provenance is required.

## Inventory

See [INDEX.md](./INDEX.md).
