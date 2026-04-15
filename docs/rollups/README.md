# Docs Rollups

**Purpose:** Define the scope of compressed active summaries under `docs/rollups/`; the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-04-15

## Scope

1. Store concise active summaries that reduce cold-start reading cost for future agents and contributors.
2. Keep raw dated history in `../history/` only for the most recent window; older provenance moves to `../archive/`.
3. Do not copy raw provenance tables into rollup docs; provenance lives in `../archive/ARCHIVE-LEDGER.md`.

## Default Load Order

1. Read [CURRENT-STATE.md](./CURRENT-STATE.md) first.
2. Jump from there to the owning SSOT in `../design/`, `../specs/`, `../review/`, or current milestone docs.
3. Read the newest raw entry in `../history/INDEX.md` or the newest active dated review bundle only if current-state detail is still insufficient.
4. Read `../archive/` only when exact historical wording or provenance trace is required.

## Inventory

See [INDEX.md](./INDEX.md).
