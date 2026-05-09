# History Docs

**Purpose:** Define the recovery usage of `docs/history/`; current repository state does not retain raw dated checkpoint logs by default.

**Last updated:** 2026-05-09

## Scope

1. Keep durable lessons in `../rollups/`, active SSOTs, and owner runbooks.
2. Keep durable workflow rules in `docs/assets/workflow/`.
3. Keep current durable rules in active SSOTs and runbooks; keep decision provenance in `docs/adr/` only while it remains directly useful.
4. Use Git history, not active docs, for exact old checkpoint text.
5. Project-level priority order still comes from [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Recovery Order

1. Read [../rollups/CURRENT-STATE.md](../rollups/CURRENT-STATE.md).
2. Read the owning `design/specs/teams/assets/workflow` documents for the topic you are touching.
3. Check [../assets/workflow/CHECKPOINT-WORKFLOW.md](../assets/workflow/CHECKPOINT-WORKFLOW.md) for the current recovery gate.
4. Use Git history only when an interrupted checkpoint still needs exact local trace details.
5. Jump to [../adr/INDEX.md](../adr/INDEX.md) or [../archive/ARCHIVE-LEDGER.md](../archive/ARCHIVE-LEDGER.md) only when current docs do not explain the surviving rule or when lifecycle provenance is required.

## Inventory

See [INDEX.md](./INDEX.md).
