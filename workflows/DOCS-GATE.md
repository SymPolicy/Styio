# Docs Gate

**Purpose:** Define the common docs/process gate entrypoint for `styio-nightly` so contributors can run owner-runbook maintenance, docs audit, and ecosystem CLI contract consistency through one command.

**Last updated:** 2026-06-28

## Goal

The repository already has individual docs tools, but delivery and CI should not wire them together ad hoc. This workflow freezes `scripts/docs-gate.sh` as the single docs/process entrypoint.

Docs changes must follow [DOCS-MAINTENANCE-WORKFLOW.md](./DOCS-MAINTENANCE-WORKFLOW.md): search existing owner documents and ADRs first, update existing files when possible, create new Markdown only with a single responsibility after `--reuse-reviewed` has been satisfied, and avoid version-style placeholders in favor of feature or transformation result names.

## Command

Local worktree docs gate:

```bash
./scripts/docs-gate.sh
```

Staged-only docs gate:

```bash
./scripts/docs-gate.sh --mode staged
```

Branch or CI docs gate:

```bash
./scripts/docs-gate.sh --mode push --base origin/main
```

## What It Runs

`scripts/docs-gate.sh` composes:

1. `python3 scripts/team-docs-gate.py`
2. `python3 scripts/docs-audit.py`
3. `python3 scripts/ecosystem-cli-doc-gate.py`

For staged or push mode, `team-docs-gate.py` receives the matching change source while `docs-audit.py` runs with `STYIO_SKIP_TEAM_DOC_GATE=1` to avoid re-running the owner gate with looser defaults.

## Scope Boundary

This gate owns docs/process health only. It does not replace:

1. `python3 scripts/repo-hygiene-gate.py` for tracked tree and commit hygiene
2. `./scripts/checkpoint-health.sh` for compiler build/test health
3. `./scripts/delivery-gate.sh` for the full delivery floor
