# Docs Archive

**Purpose:** Define the scope of archived raw docs and provenance records under `docs/archive/`; the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-04-15

## Scope

1. Store archived raw docs that have already been summarized into active docs.
2. Keep provenance ownership in [ARCHIVE-LEDGER.md](./ARCHIVE-LEDGER.md) and [ARCHIVE-MANIFEST.json](./ARCHIVE-MANIFEST.json).
3. Do not treat archive docs as default reading order; use `../rollups/` first.

## Rules

1. Move docs here only through `python3 scripts/docs-lifecycle.py cleanup ...`.
2. Archive paths mirror the original `docs/` path after dropping the leading `docs/`.
3. Archived raw docs keep their original text; lifecycle metadata lives outside the raw file body.
4. Collection inventory lives in [INDEX.md](./INDEX.md).

## Inventory

See [INDEX.md](./INDEX.md).
