# Design Docs

**Purpose:** Define the scope and naming rules for `docs/design/`; the generated file inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-07-25

## Scope

1. Store language and compiler design SSOT here.
2. Keep implementation history, review findings, and migration plans out of this directory.
3. Link to review or plan documents instead of duplicating those discussions.
4. Project-level priority order and rewrite boundary live in [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Naming Rules

1. Design-level SSOT uses stable `Styio-*.md` filenames.
2. Do not add generic files such as `notes.md`, `draft.md`, or `misc.md` here.
3. A proposal that still awaits an owner decision is not design SSOT: declare
   `**Status:** Draft`, use a filename ending in `-Draft.md`, and keep it in
   `docs/review/` unless a design-specific location is required.
4. Pre-stabilization, target-design, or `Pending implementation` wording alone
   does not make an accepted semantic owner a Draft.
5. If a topic is no longer design SSOT, move it to `docs/plan/` or `docs/review/`.

## Inventory

See [INDEX.md](./INDEX.md).
