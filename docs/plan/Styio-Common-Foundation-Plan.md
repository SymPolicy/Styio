# Styio Common Foundation Plan

**Purpose:** Define the single planning home for Styio common-foundation changes that must land before upper-layer feature, product, or ecosystem work can safely run in parallel.

**Last updated:** 2026-06-28

**Plan status:** Active foundation plan.

**Foundation key:** common-foundation

## 前置条件

1. 并行: common-foundation work should fan out by independent foundation surface first; only the merge checkpoint that changes shared contracts, gates, generated indexes, or cross-plan prerequisites is serial before dependent feature work.
2. 子智能体: sub-agents may audit or prototype independent foundation surfaces in parallel, but one owner must reconcile shared contracts, gates, and docs before dependent plans proceed.
3. 基座: this file is the owning plan for the `common-foundation` key; do not create a second plan for the same foundation key.
4. Dependency discovery: if a feature plan finds shared gate, skill, workflow, service-contract, test-harness, or compiler substrate work, move that work here first and make the feature plan depend on this plan.

## Scope

This plan owns common substrate changes that unblock multiple downstream plans:

1. workflow scheduler, reusable workflow docs, repo-local skills, and delivery/checkpoint gates;
2. docs audit, team runbook gates, generated index policy, and documentation lifecycle gates;
3. shared CTest, checkpoint-health, coverage, fuzz, parser-shadow, or security harness entrypoints;
4. compiler service contracts consumed by multiple teams, such as machine-readable diagnostics, compile-plan envelopes, source-build metadata, and runtime event shapes;
5. common source-layout, CMake, platform, toolchain, or script conventions used by more than one feature family.

It does not own feature semantics after the shared foundation is stable. Those belong in the implementation, team runbook, test catalog, or feature-specific plan.

## Parallelization Policy

1. Independent foundation lanes may run in parallel for workflow gates, docs gates, test harnesses, service-contract inventory, source-layout/CMake facts, and tool/skill registry evidence.
2. Each lane must produce a narrow patch proposal, affected-plan list, gate command, and rollback note before the shared merge checkpoint.
3. The shared merge checkpoint is serial only for published contract shapes, generated indexes/stats, workflow or registry ownership, and cross-plan prerequisite wording.
4. Upper-layer sub-agents must receive this plan as a prerequisite when their task touches a foundation surface, but they may continue read-only audits while the foundation merge gate is pending.
5. If two feature plans need the same foundation change, update this plan instead of duplicating prerequisite text in both feature plans.

## Change Routing

Use this plan before feature work when any of these surfaces change:

1. delivery, docs, workflow, skill, or checkpoint-health gates;
2. shared compile/test infrastructure;
3. public machine-readable contracts used by more than one repository or team;
4. source-build, platform, or CMake substrate shared by compiler, CLI, IDE, runtime, or ecosystem work;
5. a migration that would otherwise leave each upper-layer plan inventing its own prerequisite.

## 验收条件

1. Foundation scope, owner, and affected plans are named before upper-layer implementation starts.
2. The owning workflow, runbook, registry, and generated indexes are updated in the same checkpoint.
3. At least one gate would fail if the foundation rule or shared contract regressed.
4. Dependent feature plans list this plan in `## 前置条件` before they mark upper-layer work as parallel-ready.
5. Required validation passes: `python3 scripts/docs-audit.py`, `python3 scripts/workflow-scheduler.py check`, `python3 scripts/tool-skill-registry-gate.py`, and any owner-team gate named by the foundation change.
