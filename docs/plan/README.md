# Plan Docs

**Purpose:** Define the compact `docs/plan/` workspace for one project roadmap, three delivery stages, and their Better Plan state; generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-08-01

## Scope

1. Store only active implementation plans, active migration plans, and still-open cross-repo contracts here.
2. These files are not language or acceptance SSOT.
3. A plan may remain here after a repo-local baseline closes only if it still governs hardening, cross-repository alignment, or later closure work; when that happens, the file must state its current status explicitly near the top.
4. When a plan is superseded or its durable knowledge has been absorbed into active docs, remove it from the current tree and rely on Git history for exact old wording.
5. Store durable repository capability facts at [Capabilities.json](./Capabilities.json), Plan ownership and lifecycle summaries at [Manifest.json](./Manifest.json), and executable dependencies only in each plan directory's `Checkpoints.json`; there is no separate `better-plan` directory.
6. Store one-off reports under `docs/reports/`, not in this workspace.
7. Tradeoff order for plans still follows [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Better Plan State Model

1. `Capabilities.json` contains one observed repository root and only the examined planning branches needed by current work. Capability ancestry records structure, never execution order.
2. `Manifest.json` contains Plan objects only. Each Plan has an explicit purpose distinct from its goal and description, plus a readable kind and display policy.
3. `Checkpoints.json` contains Node objects only. Every execution edge uses a globally unique Node ID in `prerequisites`; `next` is navigation only.
4. Node roles, platforms, difficulties, and verification profiles use the current Better Plan enums. Removed legacy values such as `all`, `medium`, and `high` must not return.
5. `python3 scripts/manifest_tool.py validate docs/plan --check-sources`, readable `tree`, and `tree --details` projections must agree before a plan update is accepted.

## Required Plan Shape

Every plan document in this directory, except this README and the generated INDEX, must include these exact H2 sections:

1. `## 前置条件`
2. `## 验收条件`

`## 前置条件` must explicitly say whether work can run in parallel, whether sub-agents may be started, and whether any common foundation work must land first. Plans should default to parallel-first execution: split read-only inventory, evidence gathering, downstream confirmation, and test discovery into independent lanes; keep only shared foundation changes, semantic decisions, public contract shape changes, and final SSOT updates behind a named serial merge gate. Custom planning sections may appear between the two required sections.

`## 验收条件` must name the acceptance evidence, owner gates, or exit criteria that prove the plan is complete.

## 通用基座计划索引

The established foundation is a rule, not a perpetual active task. A feature or application-stage review that discovers shared substrate work must create one separate, independently acceptable Better Plan group for that foundation closure before dependent implementation starts.

| Capability key | Current authority | Scope |
|----------------|-------------------|-------|
| `styio-nightly/delivery-governance` | [Capabilities.json](./Capabilities.json) and [Docs / Ecosystem Runbook](../teams/DOCS-ECOSYSTEM-RUNBOOK.md) | Established workflow, documentation, test, hygiene, privacy, and release-routing rules; any future substrate change receives its own group. |

## Status Rules

1. Use explicit top-level status wording such as `Active`, `Repo-local baseline completed`, or `Completed and ready for deletion after promotion`.
2. If a repo-local baseline is complete but ecosystem closure remains open elsewhere, say so directly and link the owning master plan.
3. If a plan is still the sequencing document for unfinished work, keep it short and link the owning SSOT instead of duplicating details.
4. `docs/plan/INDEX.md` and the project roadmap should answer "is this still active?" without forcing readers to infer it from scattered stage tables.

## Naming Rules

1. Use descriptive names such as `<Topic>-Plan.md`, `<Topic>-Implementation-Plan.md`, or `<Topic>-Adjustment.md`.
2. Do not add generic filenames such as `idea.md`, `notes.md`, or `misc.md`.
3. Superseded plans are removed from the current tree after their durable rules move to the roadmap or formal owner; exact historical wording remains in Git history.

## Inventory

1. Generated plan inventory: [INDEX.md](./INDEX.md)
2. Human-readable project roadmap: [Styio-Project-Roadmap.md](./Styio-Project-Roadmap.md)
3. Better Plan capability catalog: [Capabilities.json](./Capabilities.json)
4. Better Plan manifest: [Manifest.json](./Manifest.json)
