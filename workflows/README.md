# Workflows

**Purpose:** Provide root-level reusable workflows and repo-local skills for styio-nightly delivery.

**Last updated:** 2026-06-28

## Scope

1. Keep mature reusable workflows under this root directory.
2. Treat `*.toml` as the machine-readable workflow format.
3. Keep Markdown files as human-facing explanations only.
4. Keep repo-local skills under `workflows/skills/` using `skill.toml`.
5. Pair workflow changes with gates or validation commands.
6. Use [INDEX.md](./INDEX.md) for the generated workflow inventory.

> The former `docs/assets/workflow/` mirror was retired on 2026-05-22; this root tree is the only canonical location for reusable workflow documents.

## Entry Points

Before changing code, agents must read this entrypoint and then fully read the applicable workflow Markdown/TOML pair. For functional changes, read [FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md](./FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md) before implementation planning and again before commit so targeted feature validation, upstream/downstream adaptation, and objective unable-to-verify blockers are explicit. For functional changes that replace, optimize, migrate, or broaden existing behavior, read [FEATURE-CUTOVER-WORKFLOW.md](./FEATURE-CUTOVER-WORKFLOW.md) again before final tests. For any skill, doc, workflow, script, test, config, or handoff record, run [LOCAL-INFO-LEAK-GATE.md](./LOCAL-INFO-LEAK-GATE.md) so developer-machine and server-specific details are placeholders. For syntax disputes, parse errors, EBNF mismatches, or accepted/rejected spelling questions, start with [CORRECT-SYNTAX-CONTRACT.md](./CORRECT-SYNTAX-CONTRACT.md) before editing parser, Sema, lowering, tests, or docs.

1. [FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md](./FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md)
2. [FEATURE-CUTOVER-WORKFLOW.md](./FEATURE-CUTOVER-WORKFLOW.md)
3. [ADD-REPO-FILE.md](./ADD-REPO-FILE.md)
4. [LOCAL-INFO-LEAK-GATE.md](./LOCAL-INFO-LEAK-GATE.md)
5. [ADD-RESOURCE-IDENTIFIER.md](./ADD-RESOURCE-IDENTIFIER.md)
6. [ADD-SYNTAX-WITH-SKILLS.md](./ADD-SYNTAX-WITH-SKILLS.md)
7. [CORRECT-SYNTAX-CONTRACT.md](./CORRECT-SYNTAX-CONTRACT.md)
8. [PROMOTE-NIGHTLY-PARSER-SUBSET.md](./PROMOTE-NIGHTLY-PARSER-SUBSET.md)
9. [CHANGE-BOOTSTRAP-ENV.md](./CHANGE-BOOTSTRAP-ENV.md)
10. [CHECKPOINT-WORKFLOW.md](./CHECKPOINT-WORKFLOW.md)
11. [DELIVERY-GATE.md](./DELIVERY-GATE.md)
12. [DOCS-GATE.md](./DOCS-GATE.md)
13. [FIVE-LAYER-PIPELINE.md](./FIVE-LAYER-PIPELINE.md)

## TOML Registry

See [workflows.toml](./workflows.toml).

## Inventory

See [INDEX.md](./INDEX.md).
