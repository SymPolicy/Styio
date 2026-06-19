# Styio Unified Delivery Gate

**Purpose:** Define the common delivery-floor entrypoint for Styio so contributors can run local, branch, audit, and checkpoint health checks through one safe auto command before checkpoint merge or branch delivery.

**Last updated:** 2026-06-19

## Goal

`checkpoint-health.sh` is the inner recovery/test gate, but a real delivery also needs current tool/skill inventory, repository hygiene, docs/runbook discipline, PR-range hygiene, and external audit. This workflow defines the shared floor that must run before a checkpoint merges or a branch is handed off, while delegating docs/process ownership to [DOCS-GATE.md](./DOCS-GATE.md). Tool ownership, ordering, and separation are registered in [WORKFLOW-ORCHESTRATION.md](./WORKFLOW-ORCHESTRATION.md).

## Command

Safe auto delivery floor:

```bash
./scripts/delivery-gate.sh
```

The default `auto` mode runs worktree checks when local changes exist, infers the delivery base for branch/promotion checks, runs push-range hygiene when `HEAD` is ahead of that base, then runs `styio-audit` and checkpoint health.

Explicit checkpoint delivery floor:

```bash
./scripts/delivery-gate.sh --mode checkpoint
```

Docs/process-only delivery:

```bash
./scripts/delivery-gate.sh --skip-health
```

Explicit branch delivery floor:

```bash
./scripts/delivery-gate.sh --mode push --base origin/main --range origin/main..HEAD
```

Release-candidate local floor:

```bash
./scripts/delivery-gate.sh --mode release
```

## What It Runs

`auto` mode composes:

1. `python3 scripts/workflow-scheduler.py run --profile delivery-checkpoint` when the worktree has changes
2. `python3 scripts/workflow-scheduler.py run --profile delivery-push --base <ref> --range <ref>..HEAD` when `HEAD` is ahead of the inferred base
3. `styio-audit gate --repo . --project styio`
4. `./scripts/checkpoint-health.sh --no-asan --no-fuzz`, including the 95% coverage gate

`checkpoint` mode composes:

1. `python3 scripts/workflow-scheduler.py run --profile delivery-checkpoint`
2. `styio-audit gate --repo . --project styio`
3. `./scripts/checkpoint-health.sh --no-asan --no-fuzz`, including the 95% coverage gate

`push` mode composes:

1. `python3 scripts/workflow-scheduler.py run --profile delivery-push --base <ref> --range <range>`
2. `styio-audit gate --repo . --project styio`
3. `./scripts/checkpoint-health.sh --no-asan --no-fuzz`, including the 95% coverage gate

`release` mode composes:

1. `python3 scripts/workflow-scheduler.py run --profile delivery-checkpoint`
2. `styio-audit gate --repo . --project styio`
3. `./scripts/checkpoint-health.sh` with the 95% coverage gate, ASan/UBSan, and fuzz smoke enabled

The scheduler expands those profiles into tool/skill registry validation, repository hygiene, runtime-surface alignment, team-runbook maintenance, docs audit, and ecosystem CLI doc checks in registered phase order.

## Options

Fast floor is the default. Opt in to heavier health legs only when the delivery requires them:

```bash
./scripts/delivery-gate.sh --with-asan --with-fuzz
```

Forwarded build-path options:

```bash
./scripts/delivery-gate.sh \
  --build-dir build/default \
  --asan-build-dir build/asan-ubsan \
  --fuzz-build-dir build/fuzz \
  --coverage-build-dir build/coverage \
  --coverage-threshold 95
```

## Scope Boundary

This script is the common delivery floor, not the full cutover decision by itself.

You still need the domain-specific gates from [../docs/teams/COORDINATION-RUNBOOK.md](../docs/teams/COORDINATION-RUNBOOK.md) when changing:

1. parser default route
2. IR shape consumed by codegen
3. runtime or handle contracts
4. CLI / nano contracts
5. IDE / LSP public surface

Syntax additions that reach lowering or runtime helpers must also follow [ADD-SYNTAX-WITH-SKILLS.md](./ADD-SYNTAX-WITH-SKILLS.md).

## When To Use Which Entry

1. During recovery or cold-start verification, run [CHECKPOINT-WORKFLOW.md](./CHECKPOINT-WORKFLOW.md)'s inner recovery command: `./scripts/checkpoint-health.sh`.
2. Before merging a checkpoint-sized delivery, run `./scripts/delivery-gate.sh`.
3. Before pushing or handing off a branch, run `./scripts/delivery-gate.sh`; pass `--base <ref>` only when auto cannot infer the intended delivery base.
4. Before tagging or packaging, run `./scripts/delivery-gate.sh --mode release` and keep the output with the release record.
