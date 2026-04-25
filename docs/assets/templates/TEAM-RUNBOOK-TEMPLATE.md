# Team Runbook Template

**Purpose:** Define the copyable standard format for ordinary team runbooks under `docs/teams/`.

**Last updated:** 2026-04-16

## Copying Rules

1. Copy the skeleton below into `docs/teams/<TEAM>-RUNBOOK.md`.
2. Replace the H1, `Purpose`, and `Last updated` values before delivery.
3. Keep the H2 headings exactly as written and in the same order.
4. Use H3 headings inside a required section if team-specific detail is needed.
5. Link to owning SSOT documents instead of copying full specs, test tables, or language semantics.

`COORDINATION-RUNBOOK.md` uses the coordinator-specific structure documented in [../workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md](../workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md).

## Copyable Skeleton

````markdown
# Team Name Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of OWNED_SURFACE. Keep this statement short and operational.

**Last updated:** YYYY-MM-DD

## Mission

State what this team owns in daily maintenance terms. Include one sentence naming what this runbook does not own so maintainers know which SSOT or team to use instead.

## Owned Surface

List the primary directories, generated artifacts, build targets, and test labels the team opens during normal work. Keep this to entrypoints, not a full file manifest.

Primary paths:

1. `<path>/`
2. `<path-or-file>`

Key SSOTs:

1. `Document title -> ../relative/path.md`
2. `Document title -> ../relative/path.md`

## Daily Workflow

Describe the normal edit loop in order: what to read first, which files to update together, how to regenerate artifacts, and the smallest local validation command that should run before handoff.

1. Read the owning SSOT before changing behavior.
2. Make the code, test, and documentation changes in the expected order.
3. Run the minimum local gate for this team.

## Change Classes

Classify normal changes by delivery risk. Each class should say which tests, ADR/history/checkpoint notes, or cross-team reviews are needed.

1. Small: local wording, test fixture, or isolated implementation change. Run MINIMUM_GATE.
2. Medium: behavior, generated artifact, public diagnostic, or workflow change. Update SSOT links and run EXPANDED_GATE.
3. High: semantic contract, runtime/CLI/LSP surface, repository boundary, or default-path change. Use checkpoint/release gates and coordinator review.

## Required Gates

List commands maintainers can run without reading CI configuration. Link to deeper gate catalogs instead of copying their full tables.

Minimum:

```bash
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
```

Team-specific:

```bash
TEAM_GATE_COMMAND
```

## Cross-Team Dependencies

Name upstream teams this runbook depends on, downstream teams affected by this team's changes, and review requirements for common PR classes.

1. Upstream: TEAM_OR_SSOT.
2. Downstream: TEAM_OR_SURFACE.
3. Required review: CHANGE_CLASS requires TEAM review.

## Handoff / Recovery

State what must be written before work is interrupted and how the next maintainer can resume safely.

Record:

1. Files and SSOTs changed.
2. Tests and gates already run.
3. Generated artifacts still pending.
4. Known failures, rollback point, and next owner.
````
