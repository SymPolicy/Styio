# Syntax Design

**Purpose:** Define the active syntax documentation boundary: distributed feature SSOT documents live under `features/`, while compact and cross-cutting language views stay in this directory.

**Last updated:** 2026-07-30

Generated inventory lives in [INDEX.md](./INDEX.md).

## Scope

1. Store one long-lived feature SSOT document per accepted, proposed, reserved, deferred, rejected, or superseded syntax feature under [features/](./features/).
2. Treat [ACTIVE-SYNTAX.md](./ACTIVE-SYNTAX.md), the generated syntax-feature graph, and generated indexes as composed read models rather than alternate feature authorities.
3. Keep cross-feature grammar invariants in [../Styio-EBNF.md](../Styio-EBNF.md), shared token rules in [../Styio-Symbol-Reference.md](../Styio-Symbol-Reference.md), and cross-feature semantic principles in [../Styio-Language-Design.md](../Styio-Language-Design.md).
4. Keep resource topology and safety invariants in [../Styio-Resource-Topology.md](../Styio-Resource-Topology.md).
5. A feature document may reference these shared contracts, but it owns that feature's canonical surface, semantic boundary, dependencies, prerequisites, lifecycle state, and evidence map.
6. Do not add retired-syntax catalogs here; terminal feature documents retain only the current resolution and successor/reopen edge, while exact former wording remains in Git history.

## Naming Rules

1. Use stable, searchable uppercase filenames for compact cross-feature views.
2. Use lowercase feature-id filenames under `features/`, replacing `.` with `-`.
3. Start broad authoring navigation in [ACTIVE-SYNTAX.md](./ACTIVE-SYNTAX.md), then follow its feature SSOT link.
4. Put compatibility aliases after canonical forms, not before them.

## Composition Contract

1. Feature Markdown is authoritative; the fenced `toml syntax-feature` block is machine-readable metadata embedded in the same SSOT, not a second registry.
2. `scripts/syntax-feature-state-gate.py` resolves document roles, prerequisite evidence, directed dependencies, reverse references, alternative requirements, conflicts, and lifecycle guards.
3. `SYNTAX-FEATURE-GRAPH.json` is generated from feature documents and must never be edited by hand.
4. A dependency or prerequisite change invalidates derived readiness without rewriting the feature's human decision state.
5. Delivery may advance beyond `not_started` only when the decision is `accepted` and derived readiness is `ready`.

## Inventory

See [INDEX.md](./INDEX.md).
