# Design Docs

**Purpose:** Define the scope and naming rules for `docs/design/`; the generated file inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-07-30

## Scope

1. Store language and compiler design SSOT here.
2. Keep implementation history, review findings, and migration plans out of this directory.
3. Link to review or plan documents instead of duplicating those discussions.
4. Project-level priority order and rewrite boundary live in [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Naming Rules

1. Cross-feature design SSOT uses stable `Styio-*.md` filenames.
2. Distributed syntax-feature SSOT uses lowercase feature-id filenames under
   `syntax/features/`; this is the intentional long-lived feature-document
   exception to the cross-feature naming rule.
3. Do not add generic files such as `notes.md`, `draft.md`, or `misc.md` here.
4. If a topic is no longer design SSOT, move it to `docs/plan/` or `docs/review/`.

## Inventory

See [INDEX.md](./INDEX.md).
