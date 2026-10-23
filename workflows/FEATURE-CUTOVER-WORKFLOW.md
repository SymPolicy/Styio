# Feature Cutover Workflow

**Purpose:** Require every functional change to finish the cutover from old behavior to the new canonical behavior before final testing, instead of leaving partial migration, legacy fallbacks, or old implementation paths behind.

**Last updated:** 2026-09-08

**TOML:** [FEATURE-CUTOVER-WORKFLOW.toml](./FEATURE-CUTOVER-WORKFLOW.toml) is the machine-readable workflow definition.

## Skill

Use [styio-feature-cutover/skill.toml](./skills/styio-feature-cutover/skill.toml) before final tests for any change that optimizes, replaces, migrates, or broadens an existing feature.

## Goal

A functional change is not complete when the new path merely works. It is complete when the new behavior is canonical, every intended caller uses it, tests prove the new contract, and the old implementation, compatibility path, fallback route, documentation wording, and acceptance fixtures are removed or explicitly rejected.

The old surface is the implementation explicitly superseded by this authorized change. Independently maintained assets that repository rules require to keep, such as Vityo's `prototype/`, are not migration residue. A search match alone does not authorize removal or broaden the task. Prove removal with a one-time targeted search or script, not a permanent migration gate; retain behavioral tests only where they protect the current contract.

After this cutover check, use [FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md](./FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md) to prove the changed feature can stand as a targeted, upstream/downstream verified commit unit.

## Pre-Test Self-Check

Before running final health tests, perform this self-check:

1. Name the new canonical behavior and the old behavior it replaces.
2. Search for old names, entrypoints, flags, compatibility wrappers, fallback routes, deprecated fixtures, and docs references.
3. Migrate every in-scope caller, test, doc, runbook, workflow, and generated index to the new behavior.
4. Remove old implementation paths instead of leaving them linked from the new path.
5. Rename feature, module, workflow, skill, and doc surfaces by the feature or transformation result, not by version-style names such as `v2`, `version`, `new`, `old`, `legacy`, or `latest`.
6. Replace old acceptance tests with new canonical positives and adjacent negatives that reject old spellings or routes when public compatibility is not explicitly retained.
7. Run the narrow feature tests first. Reuse that evidence in commit readiness and reserve the selected complete regression for the integrated, reviewed change.

## Allowed Exceptions

Do not silently keep old behavior. If an already-approved external contract requires a temporary adapter outside the superseded implementation, record that existing compatibility decision with:

1. the owning SSOT or runbook;
2. the public contract that still requires it;
3. a sunset or follow-up checkpoint;
4. tests proving the adapter delegates to the new canonical implementation and cannot revive the old implementation.

Creating this record does not approve new compatibility. Remove superseded adapters within the authorized migration; if an unresolved external contract prevents removal, prepare the decision and pause only the dependent cutover.

## Evidence

Every cutover report must include:

1. old paths searched and removed;
2. new canonical paths and callers;
3. retained compatibility, or `none`;
4. docs/runbooks/catalogs updated;
5. tests that prove the new path;
6. tests or searches that prove the old path is gone or rejected;
7. names that were checked for version-style drift.

## Failure Modes

Stop before final testing when any of these remain:

1. new implementation calls back into old implementation for normal behavior;
2. old and new implementations both accept the same feature without a documented compatibility decision;
3. docs still describe the old path as active;
4. tests only prove the new path works but do not prove old acceptance was removed or fail-closed;
5. a feature is split into "new path now, old cleanup later" without an explicit separate checkpoint;
6. the replacement is named with a version-style placeholder instead of the feature or transformation result.
