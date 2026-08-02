# Post-Commit CI Checks

**Purpose:** Define the required workflow for checking GitHub Actions after a local commit is pushed, including what must be verified before committing and what must be watched after pushing.

**Last updated:** 2026-06-28

## Scope

This spec applies to agent and maintainer work on `styio-nightly` branches. It covers local pre-commit verification, post-push GitHub Actions monitoring, and failure recovery for repository-local and cross-repository gates.

## Commit-Time Verification

Before creating a commit, the agent must run the closest local equivalent of the GitHub Actions checks affected by the change.

Functional changes must first complete [../../workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md](../../workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md): run targeted feature validation, verify upstream/downstream adaptation, and record every objective unable-to-verify blocker with owner, substitute evidence, and follow-up gate. Changes that replace, migrate, broaden, or retire behavior must also complete [../../workflows/FEATURE-CUTOVER-WORKFLOW.md](../../workflows/FEATURE-CUTOVER-WORKFLOW.md) before commit.

Minimum local checks for normal changes:

```bash
ctest --test-dir build/default --output-on-failure
python3 scripts/local-info-leak-gate.py --mode worktree
python3 scripts/repo-hygiene-gate.py --mode tracked
python3 scripts/docs-audit.py
```

Focused changes may use a narrower test selector only when the final handoff states the narrower scope. Cross-repository contract or product changes must also run the matching ecosystem gate from this repository, such as:

```bash
export STYIO_ECOSYSTEM_WORKSPACE=<workspace-root>
python3 scripts/ecosystem-cli-doc-gate.py --workspace-root "$STYIO_ECOSYSTEM_WORKSPACE"
python3 ../pafio-nightly/scripts/verify-ecosystem-contracts.py \
  --focused \
  --repositories-root "$STYIO_ECOSYSTEM_WORKSPACE"
```

The commit message body or handoff should record the checks that were actually run, including functional commit-readiness evidence or objective blockers.

## Post-Push Verification

After pushing a commit, the agent must actively check GitHub Actions while the current work turn remains open.

Required steps:

1. Resolve the current branch and pushed commit.
2. Query GitHub Actions for the repository and branch.
3. Watch the relevant workflow run or check suite until it reaches a terminal state when the expected runtime is reasonable.
4. If a check fails, inspect the failing job logs, identify the smallest fix, run the matching local gate, create a follow-up commit, and push again.
5. If a check is still queued or running when the turn must end, report the run URL, current status, and the command needed to resume checking.

Preferred commands:

```bash
gh run list --branch "$(git branch --show-current)" --limit 10
gh run watch <run-id> --exit-status
gh run view <run-id> --log-failed
```

If `gh` is unavailable or unauthenticated, the agent must state that GitHub Actions could not be checked directly and include the local gates that were run instead.

## Cross-Repository Work

When one delivery touches `styio-nightly`, `pafio-nightly`, and `vityo-nightly`, post-push verification applies to every pushed repository. The agent should check each repository's GitHub Actions status, not only the repository that received the last commit.

Cross-repository gates must use the same workspace checkout set that will be visible to CI. If a gate consumes another repository's branch, push that repository first or report that remote CI may still be using an older sibling checkout.

`styio-nightly` GitHub Actions check out `pafio-nightly` and `vityo-nightly` at `${{ github.ref_name }}` for cross-repository gates. A branch used for coordinated work must therefore exist in all three repositories before relying on remote Actions as the final source of truth.

## Delivery Ruleset Governance

Required GitHub merge gates are maintained through GitHub Rulesets, not legacy classic branch protection. `ai-dev` and protected release/default branches must have an active Ruleset requiring the `audit` status check from the `styio-audit` workflow, with strict required status checks enabled.

Gate audits must inspect effective branch rules, for example:

```bash
gh api repos/styio-org/styio-nightly/rules/branches/ai-dev
```

Do not use `branches/ai-dev/protection/required_status_checks` as the authority for this repository. That legacy classic endpoint can return 404 even when the Ruleset gate is active.

## Completion Criteria

A pushed change is not complete until one of these is true:

1. GitHub Actions checks passed.
2. GitHub Actions checks failed, the failure was fixed and re-pushed, and the replacement run passed.
3. GitHub Actions could not be observed within the current turn, and the final handoff records the unresolved run status and recovery command.
