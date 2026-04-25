# ADR Docs

**Purpose:** Define the conventions and scope for active, not-yet-absorbed decision records under `docs/adr/`; absorbed ADRs belong in `docs/archive/adr/`, and the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-04-08

## Scope

1. Store architecture and workflow decisions here only while their decision record still needs to stay directly discoverable.
2. Keep execution logs and recovery notes in `docs/history/`.
3. Use ADRs for ownership, lifecycle, compatibility, and workflow-boundary decisions.
4. Once the durable rule has been lifted into active design/spec/runbook/workflow docs, move the ADR to `docs/archive/adr/`.

## Conventions

1. Filenames use `ADR-XXXX-<slug>.md`.
2. Each ADR should include `Status`, `Context`, `Decision`, `Alternatives`, and `Consequences`.
3. Historical references to `new parser` map to `nightly parser` after 2026-04-07.
4. ADRs are never the default maintenance input when an active owning SSOT already exists.

## Inventory

See [INDEX.md](./INDEX.md).
