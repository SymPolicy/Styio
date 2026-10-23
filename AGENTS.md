# Repository Workflow Rules

## Branch And Pull Request Flow

- Create every commit on a task-specific temporary branch. Do not commit
  directly on `nightly` or another long-lived branch such as `main`, `stable`,
  `release`, or `ai-dev`.
- If work starts while `nightly` is checked out, create and switch to a
  temporary branch before the first commit. Use a name that identifies the
  actor and task, for example `<actor>/<task>`.
- Push temporary branches only to the downstream `origin` repository. Merge a
  temporary branch into downstream `nightly` through a pull request; never
  push it directly to `nightly`.
- Normally an upstream pull request must use downstream `nightly` as its
  head branch. Before opening it, first merge the temporary branch into
  downstream `nightly` and verify that the head is `Unka-Malloc:nightly`.
- Never push a temporary branch to the `upstream` remote.

## Local Iteration and Verification

- Reuse the task's existing branch, checkout, and build directory. Create another
  worktree only for concurrent independent work, an incompatible baseline, or to
  protect unrelated changes; a new agent turn, test run, commit, or PR update is
  not a reason to create another branch/worktree. Inspect status before reuse.
- Never reset, clean, delete, or repurpose another task's branch/worktree merely
  to simplify delivery. Long-lived branch and downstream PR rules still apply.
- Use the evidence rules in `docs/specs/POST-COMMIT-CI-CHECKS.md`. Do not rerun a
  successful check just to repeat it in another checklist or agent handoff.
- Do not add whole-repository content hashing or repeatedly checksum build
  outputs as a routine precondition for tests. Git revisions/status/diffs,
  the build system's dependency tracking, and the command's actual inputs are
  the normal evidence. Artifact integrity/security checks remain in force.

## Advisory File Coverage and CI Reports

- Discover candidate files automatically from Git and route by directory/type;
  do not require a separate per-file coverage manifest for each new file.
- Unknown routes, missing build membership, and unavailable coverage evidence
  are report-only findings. They must not create commit, CI, or merge blockers.
- Keep existing real build/test/security gates effective. File discovery does
  not prove compilation, test execution, or behavioral coverage.
- Generate the final CI report from existing outcomes/artifacts, never rerun
  tests merely to populate it. See `docs/specs/POST-COMMIT-CI-CHECKS.md` for scope.

## Approved Staged Upstream Contribution Exception

The maintainer approved the following exception on 2026-09-28 to split the
existing downstream backlog into reviewable contributions to
`SymPolicy/Styio:nightly` without rewinding the shared downstream branch.

- Downstream branches named `upstream/2026-09-28-*` may be used as upstream
  pull request heads instead of `Unka-Malloc:nightly` for this backlog.
- Build each batch from the current upstream baseline or its explicitly
  recorded predecessor. Record the source commits, dependencies, conflict
  resolutions, and validation evidence in the pull request.
- Prefer changes already merged into downstream `nightly`. Upstream-specific
  integration repairs may be made on these contribution branches; document
  them separately from the original downstream changes.
- Keep the existing upstream content and security fixes unless an explicitly
  reviewed replacement is part of the batch. Do not replace the upstream tree
  with a downstream snapshot merely to avoid conflict resolution.
- Merge batches in dependency order only after their exact candidate passes
  the required checks and relevant functional tests. Revalidate a changed
  candidate. A skipped, tolerated, or unexecuted test is not a passing test.
- This exception does not authorize direct writes to long-lived branches,
  force pushes, bypassing protection, or pushing branches to upstream.
- The normal contribution flow remains the default outside this dated batch.
