# Specs Docs

**Purpose:** Define the scope and naming rules for `docs/specs/`; the generated file inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-04-24

## Scope

1. Store contributor, agent, repository-boundary, dependency, and documentation rules here.
2. Do not store language semantics or implementation history here.
3. Prefer extending an existing spec before adding a new one.
4. Project-wide priorities and lifecycle objectives live in [PRINCIPLES-AND-OBJECTIVES.md](./PRINCIPLES-AND-OBJECTIVES.md).
5. Code audit and agent review rules live in [audit/CODE-AUDIT-CHECKLIST.md](./audit/CODE-AUDIT-CHECKLIST.md); agents must apply its seven design principles before accepting implementation work.
6. Post-push GitHub Actions checking rules live in [POST-COMMIT-CI-CHECKS.md](./POST-COMMIT-CI-CHECKS.md).
7. Technology-stack, internal-component, open-source-component, and dependency-manifest inventory maintenance rules live in [TECHNOLOGY-COMPONENT-INVENTORY.md](./TECHNOLOGY-COMPONENT-INVENTORY.md).

## Naming Rules

1. Spec documents use stable uppercase hyphenated names.
2. New specs must have a clearly separate responsibility boundary.
3. Spec changes that alter workflow should update the relevant workflow asset as well.

## Inventory

See [INDEX.md](./INDEX.md).
