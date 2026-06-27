# Functional Commit Readiness Workflow

**Purpose:** Require every functional change to become a self-contained, verified commit unit with its upstream and downstream surfaces adapted before commit or handoff.

**Last updated:** 2026-06-28

**TOML:** [FUNCTIONAL-COMMIT-READINESS-WORKFLOW.toml](./FUNCTIONAL-COMMIT-READINESS-WORKFLOW.toml) is the machine-readable workflow definition.

## Skill

Use [styio-functional-commit-readiness/skill.toml](./skills/styio-functional-commit-readiness/skill.toml) after implementing or refactoring a feature and before commit, branch handoff, final validation, or delivery-gate evidence.

## Goal

A feature change is committable only when the changed behavior can stand as one reviewable and revertible unit. The implementation, callers, upstream producers, downstream consumers, docs, tests, fixtures, workflow metadata, and owner runbooks must agree on the new behavior before the commit is treated as ready.

This workflow complements [FEATURE-CUTOVER-WORKFLOW.md](./FEATURE-CUTOVER-WORKFLOW.md): cutover proves old behavior is removed or explicitly rejected; commit readiness proves the new feature unit has been validated through the smallest useful end-to-end surface.

## Commit-Ready Self-Check

Before committing or handing off a functional change, answer these questions:

1. What single feature unit does this commit deliver?
2. What is the smallest command that proves the feature itself works?
3. Which upstream inputs, producers, configs, generators, docs, or workflow entries feed this feature?
4. Which downstream callers, consumers, CLIs, services, tests, examples, or generated artifacts depend on this feature?
5. Were those upstream and downstream surfaces updated and debugged against the new behavior?
6. Does the expected user-visible or contract-visible effect pass on the current machine?
7. Are feature, module, workflow, skill, and doc names free of version-style names and named by the feature or transformation result?

If any answer is unknown, continue implementation or debugging before committing.

## Verification Ladder

Run the smallest useful checks first, then widen only as the touched surface requires:

1. Targeted unit, fixture, script, CTest filter, or CLI smoke for the changed feature.
2. Upstream producer or input-shape check that proves the new feature receives the expected data.
3. Downstream consumer or integration smoke that proves callers work with the new behavior.
4. Owner-team gate for the affected module.
5. [FEATURE-CUTOVER-WORKFLOW.md](./FEATURE-CUTOVER-WORKFLOW.md) when behavior replaces, migrates, broadens, or retires an old route.
6. `./scripts/delivery-gate.sh --mode staged --skip-health --skip-audit` before commit when changes are staged.
7. `./scripts/delivery-gate.sh` or `./scripts/checkpoint-health.sh --no-asan --no-fuzz` for checkpoint-grade or cross-surface work.

## Objective Blockers

Unable-to-verify is acceptable only when it is objective and recorded. Examples:

1. local host cannot run a required target OS or hardware path;
2. external service, sibling repository, credential, release artifact, or owner confirmation is unavailable;
3. CI-only environment differs from the local machine and the local substitute is documented.

The record must include the skipped command or surface, the exact blocker, who or what can unblock it, the substitute evidence that was run, and the follow-up gate that must pass later.

## Commit Prompt

Before a commit, the agent must stop and confirm:

```text
Have upstream and downstream surfaces been verified against this changed feature, and does the expected behavior pass? If not, keep fixing. If it cannot be verified, record the objective blocker and owner before committing.
```

The local pre-commit hook installed by [install-repo-hygiene-hooks.sh](../scripts/install-repo-hygiene-hooks.sh) asks for this confirmation in interactive terminals. Non-interactive gates print the same prompt and require the final report to state the evidence or blocker.

## Required Evidence

Every functional commit report must include:

1. feature unit delivered;
2. upstream surfaces checked;
3. downstream surfaces checked;
4. targeted feature command and result;
5. integration or owner-team command and result;
6. cutover result, or `not applicable`;
7. unverified surfaces, or `none`;
8. objective blocker record for every unverified surface;
9. version-style naming check result.

## Failure Modes

Stop before commit when any of these remain:

1. implementation is updated but a caller still depends on old behavior;
2. upstream input shape changed but downstream consumers were not debugged;
3. the only evidence is a broad gate with no targeted feature proof;
4. tests pass through an old route while the new route is described as active;
5. an environment limitation is mentioned without command, owner, substitute evidence, and follow-up gate;
6. the change can be reverted only by undoing unrelated half-finished work;
7. the feature unit is named by `v2`, `new`, `old`, `legacy`, `latest`, or another version-style placeholder instead of the feature or transformation result.
