# Plan Docs

**Purpose:** Define the tracked Better Plan v3 workspace under `docs/plan/`, with current delivery state indexed by [Manifest.json](./Manifest.json) and unfinished product work owned by the [next-stage gap ledger](../rollups/NEXT-STAGE-GAP-LEDGER.md).

**Last updated:** 2026-09-04

## Workspace Boundary

1. `Manifest.json` is the workspace registry and uses schema `better-plan.manifest/v3`.
2. Each registered directory contains one semantic `Plan.json`, one execution-only `Checkpoints.json`, and one render-only `Plan.md` projection.
3. `Design.md` and `Design.pristine.md` are compiler inputs and provenance; they are not alternate semantic state.
4. The repository-local validator checks only v3 structure and cross-file identity:

   ```bash
   python3 scripts/manifest_tool.py validate docs/plan
   ```

5. Framework-generated delivery Markdown is exempt from repository-authored metadata and team-runbook triggers. The generated files remain tracked and reviewable.
6. `.better-plan.lock` is transient workspace coordination state and remains untracked.

## State Ownership

1. Edit semantic delivery intent only through the Better Plan framework that owns `Plan.json`.
2. Do not hand-edit `Checkpoints.json`, rendered `Plan.md`, or archived design compiler inputs.
3. Keep long-lived product backlog in [NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md), not in a second planning hierarchy. The carry-forward register includes the M7 slice, Topology migration, performance evidence, W1–W8 queues, deferred callable decisions, and the next correctness cluster.
4. Use Git history for removed planning generations and completed implementation narratives.

## Inventory

1. Generated collection inventory: [INDEX.md](./INDEX.md)
2. Plan registry: [Manifest.json](./Manifest.json)
3. Completed delivery records:
   - [PLAN-001 — restore nightly CI and migrate the plan workspace](./delivery/Plan.md)
   - [PLAN-002 — observable topology foundation](./observable-topology-foundation/Plan.md)
   - [PLAN-003 — persistent semantic IDs](./persistent-semantic-ids/Plan.md)
4. Unapproved future delivery plans:
   - [PLAN-004 — immutable observable topology snapshots](./observable-static-snapshot/Plan.md)
   - [PLAN-005 — topology delta, lineage, and queries](./observable-delta-query-lineage/Plan.md)
   - [PLAN-006 — runtime and scheduler correlation](./observable-runtime-correlation/Plan.md)
5. Explicit future-work register: [NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md#8-carry-forward-register)

PLAN-004 through PLAN-006 are planning artifacts only. They have no authorization receipt or execution checkpoints, and no Worker may start from them until each plan is separately approved.
