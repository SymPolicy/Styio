# Docs Assets

**Purpose:** Define the scope of reusable documentation assets under `docs/assets/`; the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-05-22

## Scope

1. `docs/assets/templates/` stores reusable document skeletons.
2. Do not store date-specific implementation history in this subtree.

> The former `workflows/` mirror was retired on 2026-05-22; the canonical reusable workflow documents now live at root `workflows/`. Use [`../../workflows/INDEX.md`](../../workflows/INDEX.md) as the entry point.

## Usage Rules

1. If a template can be reused across checkpoints, keep it here.
2. If the content is tied to one day or one refactor slice, keep it in history or ADR instead.
3. Update the relevant asset and its consumers together when reusable-template rules change.

## Inventory

See [INDEX.md](./INDEX.md).
