# ADR Docs

**Purpose:** Define the minimal current-tree ADR policy: implemented decisions are compressed, active rules live in owning SSOTs, and exact old decision text is recovered from Git history.

**Last updated:** 2026-06-28

## Scope

1. Store a standalone ADR here only while the decision still needs direct review.
2. Compress implemented decisions into [IMPLEMENTED-DECISIONS.md](./IMPLEMENTED-DECISIONS.md) after the durable rule moves into its owning active document.
3. Do not keep one-file-per-decision history in the current tree after implementation.
4. Use Git history for exact old wording.

## Existing ADR Search

1. Before creating an ADR, search `docs/adr/`, the owning SSOT, related runbooks, workflow docs, plans, and rollups.
2. If an active ADR already owns the decision boundary, update that ADR in place so it records the current decision only.
3. Do not create a new ADR for every small implementation change. A new ADR is allowed only when the decision boundary is genuinely new and no existing ADR or owning SSOT can carry it.
4. Do not keep old and new decisions side by side in one active ADR. Rewrite the current `Decision` and `Consequences`; exact old wording belongs to Git history.
5. If a decision is implemented and absorbed into the owning SSOT, compress it into [IMPLEMENTED-DECISIONS.md](./IMPLEMENTED-DECISIONS.md) or remove the standalone ADR from the current tree.

## Conventions

1. Temporary ADR filenames use `ADR-XXXX-<slug>.md`.
2. Each temporary ADR should include `Status`, `Context`, `Decision`, `Alternatives`, and `Consequences`.
3. ADRs are never the default maintenance input when an active owning SSOT already exists.
4. Keep this directory small; long-lived knowledge belongs in design, spec, workflow, team, test, or rollup docs.
5. Active ADR files must not add `History`, `Previous Decision`, `Superseded`, or changelog-style sections; use Git history for those layers.

## Inventory

See [INDEX.md](./INDEX.md).
