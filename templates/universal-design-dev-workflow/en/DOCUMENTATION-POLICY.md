# Documentation policy

**Purpose:** Where Markdown lives, how **SSOT** works, and how history, milestones, and tests align with automation. Not product semantics (`DOMAIN-OR-PRODUCT-SPEC.md`). **Normative:** [`./global/SSOT-AND-MAINTENANCE-RULES.md`](./global/SSOT-AND-MAINTENANCE-RULES.md).

**Last updated:** (set when forking)

## Section 0 — Principles

**0.1 Purpose line:** State early why readers open the file and what it excludes.

**0.2 Minimal change:** Prefer extending with links.

**0.3 Three-or-more:** Deduplicate; designate one SSOT; link elsewhere.

**0.4 SSOT quick reference**

| Topic | Authority | Others should |
|-------|-----------|---------------|
| Product/domain | `DOMAIN-OR-PRODUCT-SPEC.md` | Link; no long repeat |
| Dependencies | `THIRD-PARTY.md` | Match lockfiles |
| Milestone batch | `milestones/<YYYY-MM-DD>/00-Milestone-Index.md` | No forked master table |
| ADR | `architecture/ADR-NNNN-*.md` | Link only |
| Tests | `tests/TEST-CATALOG.md` | One row + command |
| Doc layout | This file | Link here |
| Contributors | `CONTRIBUTOR-AND-AGENT-SPEC.md` | Link here |
| Open questions | `OPEN-QUESTIONS-AND-HUMAN-INPUT.md` | Link until closed |

## Section 1 — History

| Rule | Detail |
|------|--------|
| Path | `history/YYYY-MM-DD.md` |
| Index | `history/README.md` |

## Section 2 — Milestones

| Rule | Detail |
|------|--------|
| Path | `milestones/<YYYY-MM-DD>/` |
| Files | `00-Milestone-Index.md`, `M*.md` |
| Acceptance | In `TEST-CATALOG.md` or **gap** |

## Section 3 — Test catalog

By area; id, input, oracle, automation; register in manifest **and** catalog.

## Section 4 — Contributor spec

On conflict with `CONTRIBUTOR-AND-AGENT-SPEC.md`, update **this policy first**.

## Section 5 — Optional checks

Tests pass; `Last updated` present; paths real or pending.
