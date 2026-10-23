# Styio Checkpoint Workflow

**Purpose:** Define checkpoint scope, interruption recovery, review and verification boundaries; language semantics remain in the owning design contracts and project priorities in the principles document.

**Last updated:** 2026-10-01

## 1. Checkpoint Scope

1. Select one independently reviewable goal: a language-feature slice, a
   functional contract closure, or a coherent test-evidence group.
2. Keep the change buildable, testable, revertible and recoverable at its delivery
   boundary. Record incomplete acceptance explicitly while work is in progress.
3. Deliver implementation and tests when affected, current documentation,
   decision rationale where needed, and enough information to resume unfinished
   work. A docs-only checkpoint does not require unrelated implementation edits.
4. Preserve existing migration/shadow gates when changing a path they protect.
   Introduce a parallel implementation only when the specific transition needs
   one and has an exit condition. New names describe responsibility; historical
   `_legacy`, `_latest` and `_draft` naming conventions are not new-code policy.
5. Store active design decisions with their owner. Use an ADR for a distinct
   unresolved or independently reviewed architecture decision; do not create a
   duplicate ADR for each routine checkpoint.
6. Behavior changes preserve the refactor template's failing-then-passing
   regression evidence where applicable; pure documentation changes use their
   document/tool checks.
7. Follow [Functional Commit Readiness](./FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md)
   and [Delivery Gate](./DELIVERY-GATE.md) for applicable verification.

## 2. Execution and Recovery

1. For work managed by the [plan workspace](../docs/plan/README.md), use its
   semantic plan and execution-checkpoint owners. Do not hand-edit generated
   `Plan.md` or duplicate the same state in a daily log.
2. If an interruption needs a separate recovery note, use
   `docs/history/YYYY-MM-DD.md` with current state, next action, reproduction
   commands, unverified gates, risks and rollback reference.
3. Resume from [CURRENT-STATE](../docs/rollups/CURRENT-STATE.md), the owning
   contract/runbook, and the active checkpoint. Run the relevant recovery checks;
   `checkpoint-health.sh` is the complete inner health gate when applicable.
4. Build directories use `build/<variant>`. Pass a requested variant explicitly;
   preserve configure/build/CTest failures and keep logs separate from path data.
5. On closure, promote durable procedures to their owner and unresolved work to
   the gap ledger. Use the documentation lifecycle workflow to remove absorbed
   recovery notes. Git history preserves the previous record.

## 3. Handoff Evidence

Record:

1. Goal, completed behavior and changed files.
2. Current contract/feature and owner.
3. Next executable step or objective blocker.
4. Source revision and exact focused build/test commands, with pass/fail/not-run.
5. Risks, migration exit conditions and rollback reference.
6. External consumer or platform acceptance still required.

## 4. Branch and Review Boundary

Use [AGENTS.md](../AGENTS.md) for the repository branch and pull-request flow,
[Repo Hygiene](./REPO-HYGIENE-COMMIT-STANDARD.md) for tracked artifacts and commit
checks, and [Post-Commit CI Checks](../docs/specs/POST-COMMIT-CI-CHECKS.md) for
publication and CI evidence. Checkpoint recovery does not itself require a
remote action or a history rewrite.

A draft or preservation checkpoint may state incomplete work. It must still
exclude build output, binaries, machine-local state and unrelated changes.
Before delivery, validate the final candidate against the applicable gate.

## 5. Related Authorities

- [Language Design](../docs/design/Styio-Language-Design.md)
- [Principles and Objectives](../docs/specs/PRINCIPLES-AND-OBJECTIVES.md)
- [Agent Specification](../docs/specs/AGENT-SPEC.md)
- [Documentation Policy](../docs/specs/DOCUMENTATION-POLICY.md)
- [Plan State Ownership](../docs/plan/README.md#state-ownership)
- [Refactor Workflow Template](../docs/assets/templates/REFACTOR-WORKFLOW-TEMPLATE.md)
