# Local Divergence Reconciliation

**Purpose:** Keep the unresolved reconciliation of previously preserved local changes visible without treating old workspace paths or feature lists as current implementation gaps.

**Last updated:** 2026-10-01

## Current Boundary

The W8 queue in [NEXT-STAGE-GAP-LEDGER](./NEXT-STAGE-GAP-LEDGER.md) remains open
because the preserved change set has not been reconciled item by item against
the current compiler revision. Its historical branch/stash locators and exact
migration narrative remain in Git history. A numbered stash slot is not a
portable or stable recovery identifier.

Several previously named feature families now have active implementations,
including the IR verifier, runtime state/scheduling, callable inference and
platform-aware IDE/LSP behavior. Their current authority is the owning feature,
source inventory and tests; they must not be described as wholly unimplemented
merely because the old reconciliation list named them.

## Reconciliation Procedure

1. Resolve an available preserved commit or immutable patch before selecting a
   slice. If the source artifact cannot be found, record that recovery blocker;
   do not reconstruct it from historical prose.
2. Compare each candidate with current source and accepted feature contracts.
   Classify it as already represented, still applicable, superseded, or awaiting
   a language/architecture decision.
3. Keep current paths and owners from [AGENT-SPEC](../specs/AGENT-SPEC.md), the
   [repository map](../specs/REPOSITORY-MAP.md), and team runbooks. In particular,
   IDE and LSP implementation currently lives under `src/StyioServices/`.
4. Preserve the registered plan workspace and current compiler/Pafio/Vityo
   boundaries. A historical path layout or deleted planning generation does not
   override current contracts.
5. Migrate an applicable implementation together with its fixtures and owner
   documentation, using the current feature and delivery gates.
6. Close W8 only when every recovered item has an explicit disposition and
   evidence. Recovery-artifact absence is a blocker, not evidence of completion.

## Validation and Handoff

Record the preserved source revision, current baseline, item disposition,
affected owner, focused test command, and unverified acceptance. Run
`python3 scripts/team-docs-gate.py` and `python3 scripts/docs-audit.py` with the
applicable source tests. Keep new durable decisions in their owning contracts;
this ledger only tracks reconciliation.
