# Post-Commit CI Checks

**Purpose:** Define the required workflow for checking GitHub Actions after a local commit is pushed, including what must be verified before committing and what must be watched after pushing.

**Last updated:** 2026-09-10

## Scope

This spec applies to agent and maintainer work on `styio-nightly` branches. It covers local pre-commit verification, post-push GitHub Actions monitoring, and failure recovery for repository-local and cross-repository gates.

Use the current request and existing approvals to determine authority. This workflow does not itself authorize a commit, push, merge, release, or governance change. Repository review and approval requirements remain effective; do not ask again for an action already covered by the same scope and risk boundary. Resolve discoverable facts and ordinary in-scope implementation issues directly, report material findings, and pause only work that needs a new decision.

## Commit-Time Verification

Before creating an authorized commit, run the closest local checks affected by the change. Use focused checks during implementation and reuse passing evidence while its inputs remain unchanged. The commands below are a scope-dependent catalog, not a requirement to run every gate before every commit.

Functional changes must first complete [../../workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md](../../workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md): run targeted feature validation, verify upstream/downstream adaptation, and record every objective unable-to-verify blocker with owner, substitute evidence, and follow-up gate. Changes that replace, migrate, broaden, or retire behavior must also complete [../../workflows/FEATURE-CUTOVER-WORKFLOW.md](../../workflows/FEATURE-CUTOVER-WORKFLOW.md) before commit.

Local checks for the affected surfaces:

```bash
ctest --test-dir build/default --output-on-failure -R '<affected-test-pattern>'
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

## Final Regression

Run the required complete regression once, after all changes, source review, in-scope repairs, and focused verification are finished. Coordinate local and CI evidence for the same candidate; required CI checks still run after an authorized push. A final complete-regression failure requires a diagnosis and concrete repair and verification proposal for the developer. Do not automatically repair, rerun, or push a repair that would restart this regression before that decision. Continue independent authorized work, and do not mark unresolved acceptance as complete.

## Post-Push Verification

After pushing a commit, the agent must actively check GitHub Actions while the current work turn remains open.

Required steps:

1. Resolve the current branch and pushed commit.
2. Query GitHub Actions for the repository and branch.
3. Observe the relevant run for the exact pushed commit using bounded tool waits and progress updates. An expired observation window does not cancel the run or establish its result.
4. If a check fails, inspect its diagnostics and report a privacy-safe cause and the smallest repair and verification proposal. Follow the Final Regression decision rule for complete-regression failures. Ordinary focused-check failures may be repaired within existing authority; a follow-up commit or push must also be covered by that authority.
5. If observation is blocked or the turn ends before the run completes, report the run URL, commit, unresolved status, and resume command as an incomplete verification handoff.

Preferred commands:

```bash
gh run list --branch "$(git branch --show-current)" --limit 10
gh run view <run-id> --json headSha,status,conclusion,url
gh run view <run-id> --log-failed
```

If `gh` is unavailable or unauthenticated, the agent must state that GitHub Actions could not be checked directly and include the local gates that were run instead.

## Cross-Repository Work

When one delivery touches `styio-nightly`, `pafio-nightly`, and `vityo-nightly`, post-push verification applies to every pushed repository. The agent should check each repository's GitHub Actions status, not only the repository that received the last commit.

Cross-repository gates must use the same workspace checkout set that will be visible to CI. If a gate consumes another repository's branch, perform an already-authorized dependency push first; otherwise prepare the required handoff and report the revision mismatch. A gate dependency does not grant permission to publish another repository.

`styio-nightly` GitHub Actions resolve the ecosystem lane from the pull request target branch, or from the pushed branch. Temporary pull request branches targeting `nightly` therefore consume the siblings' `nightly` branches. Publish any required sibling changes to that lane before relying on the cross-repository checks.

## Delivery Ruleset Governance

Required GitHub merge gates are maintained through GitHub Rulesets, not legacy classic branch protection. The protected integration branch is `nightly`. Its active Ruleset must use strict required status checks and preserve the verified check-run identities: `platform-adaptation / linux-ci-gate`, `platform-adaptation / macos-ci-gate`, `test / smoke`, `test / golden-standard`, `styio-audit`, and `hygiene`. Report-only Windows, coverage, and benchmark observability jobs are not required checks until a separate governance change approves them.

Gate audits must inspect effective branch rules, for example:

```bash
gh api repos/Unka-Malloc/styio-nightly/rulesets
```

Do not use `branches/nightly/protection/required_status_checks` as the authority for this repository. That classic endpoint can return 404 even when the Ruleset gate is active.

## Completion Criteria

A delivery requiring remote verification is complete only when the required checks pass for the exact delivered commit and all authorized acceptance conditions are satisfied. After an approved repair and push, use the replacement commit's results.

Queued, running, failed, cancelled, or unobservable checks remain unresolved verification. A status URL and recovery command make the handoff actionable; they do not make the delivery complete. A local-only request is complete against its local acceptance conditions and does not require an unsolicited push.
