# Specs Docs

**Purpose:** Define the scope and naming rules for `docs/specs/`; the generated file inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-11-01

## Scope

1. Store contributor, agent, repository-boundary, dependency, and documentation rules here.
2. Do not store language semantics or implementation history here.
3. Prefer extending an existing spec before adding a new one.
4. Project-wide priorities and lifecycle objectives live in [PRINCIPLES-AND-OBJECTIVES.md](./PRINCIPLES-AND-OBJECTIVES.md).
5. Code review and defect-triage rules live in the team runbooks under [../teams/INDEX.md](../teams/INDEX.md); design-principle priorities stay anchored in [PRINCIPLES-AND-OBJECTIVES.md](./PRINCIPLES-AND-OBJECTIVES.md) and delivery gates enforce them mechanically.
6. Post-push GitHub Actions checking rules live in [POST-COMMIT-CI-CHECKS.md](./POST-COMMIT-CI-CHECKS.md).
7. Technology-stack, internal-component, open-source-component, and dependency-manifest inventory maintenance rules live in [TECHNOLOGY-COMPONENT-INVENTORY.md](./TECHNOLOGY-COMPONENT-INVENTORY.md).

## Naming Rules

1. Spec documents use stable uppercase hyphenated names.
2. New specs must have a clearly separate responsibility boundary.
3. Spec changes that alter workflow should update the relevant workflow asset as well.

## Inventory

See [INDEX.md](./INDEX.md).
