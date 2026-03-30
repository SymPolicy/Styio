# Workflow map

**Purpose:** Phases from intent to release. Rules: [`./global/SSOT-AND-MAINTENANCE-RULES.md`](./global/SSOT-AND-MAINTENANCE-RULES.md).

**Last updated:** (set when forking)

## Phases

| Phase | Inputs | Outputs | Gate |
|-------|--------|---------|------|
| 0 Charter | Problem, non-goals, compliance | Open questions filled | Named owners |
| 1 Domain | Journeys, invariants | `DOMAIN-OR-PRODUCT-SPEC.md` | No contra-spec work |
| 2 Architecture | Quality, deployment | ADRs, diagrams | Build/test documented |
| 3 Milestones | Slices, dependencies | `milestones/<date>/` | Acceptance mappable |
| 4 Tests | Acceptance, CI | `TEST-CATALOG.md` | No phantom tests |
| 5 Implementation | Specs, catalog | Code, history | SSOT respected |
| 6 Release | Changelog | Tags, history | Open questions triaged |

## Artifact graph

```mermaid
flowchart LR
  OQ[OPEN_QUESTIONS] --> DS[DOMAIN_SPEC]
  DS --> MS[MILESTONES]
  MS --> TC[TEST_CATALOG]
  TC --> IMPL[IMPLEMENTATION]
  IMPL --> H[history_logs]
  DP[DOC_POLICY] -.-> DS
  DP -.-> MS
  DP -.-> TC
  CAS[CONTRIBUTOR_SPEC] -.-> IMPL
```

Stall on human judgment: return to `OPEN-QUESTIONS-AND-HUMAN-INPUT.md`.
