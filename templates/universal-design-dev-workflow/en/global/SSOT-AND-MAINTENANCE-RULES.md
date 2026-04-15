# SSOT and maintenance rules (normative)

**Purpose:** Documentation hygiene. `DOCUMENTATION-POLICY.md` in this tree should stay aligned unless the project overrides in writing.

## Document purpose line

Early in each file: **why** readers open it and **what** it excludes.

## Minimal change

Extend sections and link before adding parallel long documents.

## Three-or-more rule

If **three or more** documents substantially repeat the same fact:

1. Designate **one SSOT**.
2. Add a new focused document only when none fits.
3. Remove duplication elsewhere; replace with links.

## SSOT slots (customize when forking)

| Topic | Authority | Others should |
|-------|-----------|---------------|
| Product/domain | `DOMAIN-OR-PRODUCT-SPEC.md` | Link; short summary |
| Dependencies | `THIRD-PARTY.md` | Match lockfiles |
| Milestone batch | `milestones/<YYYY-MM-DD>/00-Milestone-Index.md` | No ad hoc duplicate tables |
| Architecture | `architecture/ADR-NNNN-*.md` | Link only |
| Tests | `tests/TEST-CATALOG.md` | One row + exact command |
| Doc topology | `DOCUMENTATION-POLICY.md` | Link here |
| Contributors | `CONTRIBUTOR-AND-AGENT-SPEC.md` | Link here |
| Open questions | `OPEN-QUESTIONS-AND-HUMAN-INPUT.md` | Link until closed |

## History

- Path: `history/YYYY-MM-DD.md`
- Index: `history/README.md`

## Milestones

- Path: `milestones/<YYYY-MM-DD>/`
- Files: `00-Milestone-Index.md`, segment `M*.md`
- Acceptance: named in `TEST-CATALOG.md` or **gap**

## Test catalog

- Group by functional area.
- Row: test id, input, oracle, automation.
- Register in **one** CI/build manifest **and** the catalog.

## Policy vs contributor spec

If `DOCUMENTATION-POLICY.md` and `CONTRIBUTOR-AND-AGENT-SPEC.md` disagree, fix **policy and indexes first**.

## Optional checks

- Documented tests pass.
- `Last updated` on history/milestone indexes.
- `TEST-CATALOG.md` paths exist or marked pending.
