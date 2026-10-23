# Docs Archive

**Purpose:** Define the remaining archive lifecycle shell under `docs/archive/`; historical prose and retired syntax snapshots live in Git history, while the current tree keeps active docs and minimal provenance metadata.

**Last updated:** 2026-05-22

## Scope

1. Keep only lifecycle metadata and collection entrypoints needed by docs tooling.
2. Do not retain historical source snapshots, retired syntax catalogs, old planning batches, old plans, old rollups, archived examples, or deprecated implementation source in the current repository tree.
3. Keep provenance ownership in [ARCHIVE-LEDGER.md](./ARCHIVE-LEDGER.md) and [ARCHIVE-MANIFEST.json](./ARCHIVE-MANIFEST.json).
4. Use `../rollups/`, active SSOTs, and team runbooks first; use Git history only when exact historical wording is required.
5. Do not copy historical syntax from Git history into active examples without checking the active language SSOT.
6. ADR provenance is compressed into [`../adr/IMPLEMENTED-DECISIONS.md`](../adr/IMPLEMENTED-DECISIONS.md) rather than retained as one-file-per-decision under `docs/archive/adr/`. The former `docs/archive/adr/` shell was retired on 2026-05-22.

## Subdirectories

`docs/archive/history/` and `docs/archive/review/` are lifecycle archive targets used by `python3 scripts/docs-lifecycle.py mark` / `cleanup`. They are intentionally empty in the current tree; their `README.md`/`INDEX.md` files exist only so the docs-index generator can keep the collection contract intact. Do not commit raw historical content to either subdirectory.

## Rules

1. Use `python3 scripts/docs-lifecycle.py validate` to verify the lifecycle metadata shell.
2. Do not add new archive-only prose unless it is required by docs tooling or an active policy explicitly asks for it.
3. If an old document is needed, retrieve it from Git history and promote only the still-valid rule into the owning active SSOT.
4. Collection inventory lives in [INDEX.md](./INDEX.md).

## Inventory

See [INDEX.md](./INDEX.md).
