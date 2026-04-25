# Styio Unified Delivery Gate

**Purpose:** Define the common delivery-floor entrypoint for Styio so contributors can run repository hygiene, team-runbook maintenance, docs audit, and checkpoint health through one command before checkpoint merge or branch delivery.

**Last updated:** 2026-04-26

## Goal

`checkpoint-health.sh` is the inner recovery/test gate, but a real delivery also needs repository hygiene and docs/runbook discipline. This workflow defines the shared floor that must run before a checkpoint merges or a branch is handed off.

## Command

Checkpoint delivery floor:

```bash
./scripts/delivery-gate.sh --mode checkpoint
```

Branch delivery floor:

```bash
./scripts/delivery-gate.sh --mode push --base origin/main
```

Docs/process-only delivery:

```bash
./scripts/delivery-gate.sh --mode checkpoint --skip-health
```

## What It Runs

`checkpoint` mode composes:

1. `python3 scripts/repo-hygiene-gate.py --mode staged`
2. `python3 scripts/runtime-surface-gate.py`
3. `python3 scripts/team-docs-gate.py --mode staged`
4. `python3 scripts/docs-audit.py`
5. `./scripts/checkpoint-health.sh --no-asan --no-fuzz`

`push` mode composes:

1. `python3 scripts/repo-hygiene-gate.py --mode push`
2. `python3 scripts/runtime-surface-gate.py`
3. `python3 scripts/team-docs-gate.py --base <ref>` where `<ref>` comes from `--base` or the branch upstream
4. `python3 scripts/docs-audit.py`
5. `./scripts/checkpoint-health.sh --no-asan --no-fuzz`

## Options

Fast floor is the default. Opt in to heavier health legs only when the delivery requires them:

```bash
./scripts/delivery-gate.sh --mode checkpoint --with-asan --with-fuzz
```

Forwarded build-path options:

```bash
./scripts/delivery-gate.sh \
  --mode checkpoint \
  --build-dir build-codex \
  --asan-build-dir build-asan-ubsan \
  --fuzz-build-dir build-fuzz
```

## Scope Boundary

This script is the common delivery floor, not the full cutover decision by itself.

You still need the domain-specific gates from [../../teams/COORDINATION-RUNBOOK.md](../../teams/COORDINATION-RUNBOOK.md) when changing:

1. parser default route
2. IR shape consumed by codegen
3. runtime or handle contracts
4. CLI / nano contracts
5. IDE / LSP public surface

Syntax additions that reach lowering or runtime helpers must also follow [SYNTAX-ADDITION-WORKFLOW.md](./SYNTAX-ADDITION-WORKFLOW.md).

## When To Use Which Entry

1. During recovery or cold-start verification, run [CHECKPOINT-WORKFLOW.md](./CHECKPOINT-WORKFLOW.md)'s inner recovery command: `./scripts/checkpoint-health.sh`.
2. Before merging a checkpoint-sized delivery, run `./scripts/delivery-gate.sh --mode checkpoint`.
3. Before pushing or handing off a branch, run `./scripts/delivery-gate.sh --mode push --base <ref>`.
