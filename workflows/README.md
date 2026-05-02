# Workflows

**Purpose:** Provide root-level reusable workflows and repo-local skills for styio-nightly delivery.

**Last updated:** 2026-05-01

## Scope

1. Keep mature reusable workflows under this root directory.
2. Treat `*.toml` as the machine-readable workflow format.
3. Keep Markdown files as human-facing explanations only.
4. Keep repo-local skills under `workflows/skills/` using `skill.toml`.
5. Keep docs mirrors under `docs/assets/workflow/` available for existing links.
6. Pair workflow changes with gates or validation commands.

## Entry Points

Before changing code, agents must read this entrypoint and then fully read the applicable workflow Markdown/TOML pair. For syntax disputes, parse errors, EBNF mismatches, or accepted/rejected spelling questions, start with [CORRECT-SYNTAX-CONTRACT.md](./CORRECT-SYNTAX-CONTRACT.md) before editing parser, Sema, lowering, tests, or docs.

1. [ADD-REPO-FILE.md](./ADD-REPO-FILE.md)
2. [ADD-RESOURCE-IDENTIFIER.md](./ADD-RESOURCE-IDENTIFIER.md)
3. [ADD-SYNTAX-WITH-SKILLS.md](./ADD-SYNTAX-WITH-SKILLS.md)
4. [CORRECT-SYNTAX-CONTRACT.md](./CORRECT-SYNTAX-CONTRACT.md)
5. [PROMOTE-NIGHTLY-PARSER-SUBSET.md](./PROMOTE-NIGHTLY-PARSER-SUBSET.md)
6. [CHANGE-BOOTSTRAP-ENV.md](./CHANGE-BOOTSTRAP-ENV.md)
7. [CHECKPOINT-WORKFLOW.md](./CHECKPOINT-WORKFLOW.md)
8. [DELIVERY-GATE.md](./DELIVERY-GATE.md)
9. [DOCS-GATE.md](./DOCS-GATE.md)
10. [FIVE-LAYER-PIPELINE.md](./FIVE-LAYER-PIPELINE.md)

## TOML Registry

See [workflows.toml](./workflows.toml).

## Inventory

See [INDEX.md](./INDEX.md).
