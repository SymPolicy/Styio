# External Docs

**Purpose:** Define the scope and naming rules for external-facing service and handoff docs consumed by tools, editors, IDEs, package managers, and sibling Styio repositories.

**Last updated:** 2026-04-23

## Scope

1. Keep general language-service surfaces in [SERVICES.md](./SERVICES.md).
2. Keep sibling-repository handoff material under `for-*` subdirectories.
3. Keep repository-local semantics in the owning `docs/design/`, `docs/specs/`, or `docs/plan/` SSOT.

## Naming Rules

1. Use `SERVICES.md` for consumer-neutral services under `src/StyioServices/`.
2. Use `for-<repo-or-consumer>/` for handoff collections.
3. Regenerate [INDEX.md](./INDEX.md) after changes.

## Inventory

See [INDEX.md](./INDEX.md).
