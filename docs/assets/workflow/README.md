# Workflow Assets

**Purpose:** Define the scope of reusable workflow documents under `docs/assets/workflow/`; the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-05-01

## Scope

1. Store reusable engineering workflows, test-framework guides, and repository hygiene rules here.
2. Keep one-off execution logs out of this directory.
3. Workflow changes should be reflected in automation and validation entry points.

## Usage Rules

Agents must inspect this directory and read the applicable workflow before editing implementation. For syntax disputes, parser/Sema mismatch, EBNF drift, or accepted/rejected spelling questions, start with [CORRECT-SYNTAX-CONTRACT.md](./CORRECT-SYNTAX-CONTRACT.md).

1. Add a workflow asset only if multiple checkpoints or contributors will reuse it.
2. Pair workflow changes with verification commands or gates.
3. Keep workflow assets discoverable from `docs/specs/DOCUMENTATION-POLICY.md`.
4. Workflow assets must not redefine project-wide priorities; link to [../../specs/PRINCIPLES-AND-OBJECTIVES.md](../../specs/PRINCIPLES-AND-OBJECTIVES.md) when tradeoffs matter.
5. Register every active workflow asset and runnable gate in [WORKFLOW-ORCHESTRATION.md](./WORKFLOW-ORCHESTRATION.md) through `scripts/workflow-scheduler.py`.

## Inventory

See [INDEX.md](./INDEX.md).
