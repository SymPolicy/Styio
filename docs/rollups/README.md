# Docs Rollups

**Purpose:** Define the scope of compressed active summaries under `docs/rollups/`; these files are the first stop for active repository state, while history and archive stay optional provenance only.

**Last updated:** 2026-10-01

## Scope

1. Store concise active summaries that reduce cold-start reading cost for future agents and contributors.
2. Keep only needed recovery notes in `../history/`. After their durable content is promoted, remove the active copy through lifecycle tooling; exact prose remains in Git history.
3. Do not copy raw provenance tables into rollup docs; provenance lives in `../archive/ARCHIVE-LEDGER.md`.

## Default Load Order

1. Read [CURRENT-STATE.md](./CURRENT-STATE.md) first.
2. Jump from there to the owning SSOT in `../design/`, `../specs/`, `../teams/`, `../../workflows/`, or the current active plan docs.
3. Read the newest raw entry in `../history/INDEX.md` or the newest active dated review bundle only if active docs are still insufficient.
4. Read `../archive/` for lifecycle metadata. Use Git history for exact historical wording.

## Inventory

See [INDEX.md](./INDEX.md).
