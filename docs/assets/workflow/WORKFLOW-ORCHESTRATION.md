# Workflow Orchestration

**Purpose:** Define the registered workflow documents, tool responsibilities, ordering rules, and scheduler entrypoints that keep Styio delivery workflows separated and executable.

**Last updated:** 2026-04-26

## Separation Rules

1. Every active `docs/assets/workflow/*.md` file, except `README.md` and generated `INDEX.md`, must be registered in `scripts/workflow-scheduler.py`.
2. Each workflow document owns exactly one responsibility boundary. A new workflow must not restate another workflow's gate details; it links to the owner instead.
3. Each tool owns exactly one validation responsibility. Profiles may reuse a tool, but two tools must not check the same responsibility.
4. Profiles must run tools in ascending phase order. Earlier phases may prepare evidence; later phases must not redefine earlier checks.
5. A delivery workflow is not complete until the scheduler registry, workflow docs, CI entrypoint, and delivery entrypoint agree.

## Audit Table

The generated source of truth is:

```bash
python3 scripts/workflow-scheduler.py list --format markdown
```

Current registered table:

| Type | Key | Phase | Owner | Responsibility | Command / Path |
|------|-----|-------|-------|----------------|----------------|
| Workflow | `workflow-orchestration` | 5 | docs | Tool registry, workflow separation, profile ordering, and scheduler usage. | `docs/assets/workflow/WORKFLOW-ORCHESTRATION.md` |
| Workflow | `repo-hygiene` | 10 | docs | Repository cleanliness, commit, push, and history rewriting standards. | `docs/assets/workflow/REPO-HYGIENE-COMMIT-STANDARD.md` |
| Workflow | `syntax-addition` | 25 | docs | Ordered syntax-change chain from language SSOT through runtime registration. | `docs/assets/workflow/SYNTAX-ADDITION-WORKFLOW.md` |
| Workflow | `docs-maintenance` | 30 | docs | Documentation metadata, generated indexes, and archive lifecycle. | `docs/assets/workflow/DOCS-MAINTENANCE-WORKFLOW.md` |
| Workflow | `team-runbook-maintenance` | 30 | docs | Team runbook ownership and update requirements for touched surfaces. | `docs/assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md` |
| Workflow | `five-layer-pipeline` | 45 | docs | Layered compiler golden coverage from lexer through runtime output. | `docs/assets/workflow/FIVE-LAYER-PIPELINE.md` |
| Workflow | `test-catalog` | 45 | docs | Named test inventory, fixtures, labels, and automation evidence. | `docs/assets/workflow/TEST-CATALOG.md` |
| Workflow | `checkpoint` | 50 | docs | Recovery-oriented checkpoint sizing and interruption handling. | `docs/assets/workflow/CHECKPOINT-WORKFLOW.md` |
| Workflow | `delivery-gate` | 60 | docs | Common delivery-floor entrypoint and health-gate handoff. | `docs/assets/workflow/DELIVERY-GATE.md` |
| Tool | `workflow-scheduler-check` | 5 | Docs / Ecosystem | Validate workflow registry, separation, ordering, and discoverability. | `python3 scripts/workflow-scheduler.py check` |
| Tool | `workflow-scheduler-tests` | 6 | Test Quality | Run scheduler unit tests for range resolution and registry invariants. | `python3 tests/workflow_scheduler_test.py` |
| Tool | `repo-hygiene-push` | 10 | Docs / Ecosystem | Reject forbidden artifacts introduced by the incoming revision range. | `python3 scripts/repo-hygiene-gate.py --mode push --range {range}` |
| Tool | `repo-hygiene-staged` | 10 | Docs / Ecosystem | Reject forbidden staged artifacts before checkpoint delivery. | `python3 scripts/repo-hygiene-gate.py --mode staged` |
| Tool | `repo-hygiene-tracked` | 10 | Docs / Ecosystem | Reject forbidden tracked artifacts and repository cleanliness drift. | `python3 scripts/repo-hygiene-gate.py --mode tracked` |
| Tool | `runtime-surface` | 20 | Codegen / Runtime | Keep codegen helper calls, ExternLib exports, implementations, and ORC registrations aligned. | `python3 scripts/runtime-surface-gate.py` |
| Tool | `team-docs-base` | 30 | Docs / Ecosystem | Require runbook updates for branch changes against a base ref. | `python3 scripts/team-docs-gate.py --base {base}` |
| Tool | `team-docs-staged` | 30 | Docs / Ecosystem | Require runbook updates for staged checkpoint changes. | `python3 scripts/team-docs-gate.py --mode staged` |
| Tool | `team-docs-worktree` | 30 | Docs / Ecosystem | Require runbook updates for owned worktree changes. | `python3 scripts/team-docs-gate.py` |
| Tool | `docs-audit` | 35 | Docs / Ecosystem | Validate docs metadata, links, lifecycle state, and generated-index freshness. | `python3 scripts/docs-audit.py` |
| Tool | `ecosystem-cli-docs` | 40 | Docs / Ecosystem | Validate local ecosystem CLI contract documentation. | `python3 scripts/ecosystem-cli-doc-gate.py` |
| Tool | `ecosystem-cli-docs-workspace` | 40 | Docs / Ecosystem | Validate ecosystem CLI contract docs against checked-out sibling repositories. | `python3 scripts/ecosystem-cli-doc-gate.py --require-workspace --json` |

## Profiles

| Profile | Scope | Ordered Call Chain |
|---------|-------|--------------------|
| `syntax-local` | Local syntax-change worktree verification. | scheduler check -> scheduler tests -> tracked hygiene -> runtime surface -> team docs -> docs audit -> ecosystem CLI docs |
| `delivery-checkpoint` | Process gates before checkpoint health. | staged hygiene -> runtime surface -> staged team docs -> docs audit -> ecosystem CLI docs |
| `delivery-push` | Branch handoff against a base ref. | push hygiene -> runtime surface -> base team docs -> docs audit -> ecosystem CLI docs |
| `ci-prebuild` | GitHub Actions prebuild checks before compile/test. | scheduler check -> scheduler tests -> tracked hygiene -> incoming history hygiene -> runtime surface -> base team docs -> workspace ecosystem CLI docs |

## Required Commands

Validate registry and ordering:

```bash
python3 scripts/workflow-scheduler.py check
```

Run local syntax workflow checks:

```bash
python3 scripts/workflow-scheduler.py run --profile syntax-local
```

Run branch delivery checks:

```bash
python3 scripts/workflow-scheduler.py run --profile delivery-push --base origin/main --range origin/main..HEAD
```

## Adding A Workflow

1. Search `docs/assets/workflow/` and this registry before creating a new workflow.
2. If no existing workflow owns the responsibility, add the new document and register it in `WORKFLOW_DOCS`.
3. Add or reuse tools in `TOOLS`, preserving one responsibility per tool.
4. Add the tool to a profile only where the profile's scope requires it.
5. Run `python3 scripts/workflow-scheduler.py check`, `python3 tests/workflow_scheduler_test.py`, and `python3 scripts/docs-index.py --write`.
