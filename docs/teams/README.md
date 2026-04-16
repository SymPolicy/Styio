# Team Runbooks

**Purpose:** Define the scope, naming, and maintenance rules for `docs/teams/`; generated inventory lives in [INDEX.md](./INDEX.md), while language, test, workflow, and repository-boundary truth remains in the owning SSOT documents.

**Last updated:** 2026-04-16

## Scope

1. Store short daily-work runbooks for Styio maintenance teams.
2. Provide handoff, review, and verification entrypoints for each team.
3. Keep cross-team coordination rules in [COORDINATION-RUNBOOK.md](./COORDINATION-RUNBOOK.md).
4. Do not redefine language semantics, compiler behavior, test catalogs, package contracts, or repository boundaries here.

## Naming Rules

1. Team files use `<TEAM>-RUNBOOK.md`.
2. The shared coordinator entrypoint is `COORDINATION-RUNBOOK.md`.
3. Collection statistics live in [DOC-STATS.md](./DOC-STATS.md).
4. Each runbook must link to owning SSOT documents instead of copying full specs or test tables.
5. When a team boundary changes, update the affected runbook, [COORDINATION-RUNBOOK.md](./COORDINATION-RUNBOOK.md), and [DOC-STATS.md](./DOC-STATS.md) when document size changes materially, then regenerate [INDEX.md](./INDEX.md).

## Fixed Runbook Shape

Each team runbook must follow [../assets/templates/TEAM-RUNBOOK-TEMPLATE.md](../assets/templates/TEAM-RUNBOOK-TEMPLATE.md). The delivery gate checks the H1, top-level `Purpose`, top-level `Last updated`, and exact H2 section order so maintainers can fix format failures without reading gate code.

Required sections:

1. Mission
2. Owned Surface
3. Daily Workflow
4. Change Classes
5. Required Gates
6. Cross-Team Dependencies
7. Handoff / Recovery

[COORDINATION-RUNBOOK.md](./COORDINATION-RUNBOOK.md) uses a coordinator-specific shape documented in [../assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md](../assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md), because it owns module maps and review policy rather than a single team surface.

## Maintenance Gate

Every delivery that changes a mapped team-owned folder must update that team's runbook. The gate is documented in [../assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md](../assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md) and runs through:

```bash
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
```

## Inventory

See [INDEX.md](./INDEX.md).
