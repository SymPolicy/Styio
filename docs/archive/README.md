# Docs Archive

**Purpose:** Define the scope of archived provenance under `docs/archive/`; this collection centralizes absorbed history, milestones, plans, ADRs, archived rollups, and lifecycle records so active maintenance docs can stay small and current.

**Last updated:** 2026-04-16

## Scope

1. Store archived raw docs that have already been summarized into active docs.
2. Store absorbed milestone batches, superseded or completed plans, absorbed ADRs, and archived provenance rollups after their durable rules have been lifted into active docs.
3. Keep provenance ownership in [ARCHIVE-LEDGER.md](./ARCHIVE-LEDGER.md) and [ARCHIVE-MANIFEST.json](./ARCHIVE-MANIFEST.json).
4. Do not treat archive docs as default reading order; use `../rollups/`, active SSOTs, and team runbooks first.

## Rules

1. Use `python3 scripts/docs-lifecycle.py cleanup ...` for supported raw-history/review lifecycle moves.
2. Use explicit archive migrations for absorbed milestone batches, plans, and ADRs once their durable rules have been lifted into active docs.
3. Archive paths mirror the original `docs/` path after dropping the leading `docs/` where practical.
4. Archived raw docs keep their original text; lifecycle metadata lives outside the raw file body.
5. Collection inventory lives in [INDEX.md](./INDEX.md).

## Inventory

See [INDEX.md](./INDEX.md).
