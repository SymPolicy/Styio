# ADR Docs

**Purpose:** Define the minimal current-tree ADR policy: implemented decisions are compressed, active rules live in owning SSOTs, and exact old decision text is recovered from Git history.

**Last updated:** 2026-05-09

## Scope

1. Store a standalone ADR here only while the decision still needs direct review.
2. Compress implemented decisions into [IMPLEMENTED-DECISIONS.md](./IMPLEMENTED-DECISIONS.md) after the durable rule moves into its owning active document.
3. Do not keep one-file-per-decision history in the current tree after implementation.
4. Use Git history for exact old wording.

## Conventions

1. Temporary ADR filenames use `ADR-XXXX-<slug>.md`.
2. Each temporary ADR should include `Status`, `Context`, `Decision`, `Alternatives`, and `Consequences`.
3. ADRs are never the default maintenance input when an active owning SSOT already exists.
4. Keep this directory small; long-lived knowledge belongs in design, spec, workflow, team, test, or rollup docs.

## Inventory

See [INDEX.md](./INDEX.md).
