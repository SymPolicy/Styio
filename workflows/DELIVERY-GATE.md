# Styio Unified Delivery Gate

**Purpose:** Define the common delivery-floor entrypoint for Styio so contributors can run repository hygiene, the unified docs gate, and checkpoint health through one command before checkpoint merge or branch delivery.

**Last updated:** 2026-04-16

## Goal

`checkpoint-health.sh` is the inner recovery/test gate, but a real delivery also needs repository hygiene and docs/runbook discipline. This workflow defines the shared floor that must run before a checkpoint merges or a branch is handed off, while delegating docs/process checks to [DOCS-GATE.md](./DOCS-GATE.md).

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
2. `./scripts/docs-gate.sh --mode staged`
3. `./scripts/checkpoint-health.sh --no-asan --no-fuzz`

`push` mode composes:

1. `python3 scripts/repo-hygiene-gate.py --mode push`
2. `./scripts/docs-gate.sh --mode push --base <ref>` where `<ref>` comes from `--base` or the branch upstream
3. `./scripts/checkpoint-health.sh --no-asan --no-fuzz`

## Options

Fast floor is the default. Opt in to heavier health legs only when the delivery requires them:

```bash
./scripts/delivery-gate.sh --mode checkpoint --with-asan --with-fuzz
```

Forwarded build-path options:

```bash
./scripts/delivery-gate.sh \
  --mode checkpoint \
  --build-dir build/default \
  --asan-build-dir build/asan-ubsan \
  --fuzz-build-dir build/fuzz
```

## Scope Boundary

This script is the common delivery floor, not the full cutover decision by itself.

You still need the domain-specific gates from [../docs/teams/COORDINATION-RUNBOOK.md](../docs/teams/COORDINATION-RUNBOOK.md) when changing:

1. parser default route
2. IR shape consumed by codegen
3. runtime or handle contracts
4. CLI / nano contracts
5. IDE / LSP public surface

## When To Use Which Entry

1. During recovery or cold-start verification, run [CHECKPOINT-WORKFLOW.md](./CHECKPOINT-WORKFLOW.md)'s inner recovery command: `./scripts/checkpoint-health.sh`.
2. Before merging a checkpoint-sized delivery, run `./scripts/delivery-gate.sh --mode checkpoint`.
3. Before pushing or handing off a branch, run `./scripts/delivery-gate.sh --mode push --base <ref>`.
