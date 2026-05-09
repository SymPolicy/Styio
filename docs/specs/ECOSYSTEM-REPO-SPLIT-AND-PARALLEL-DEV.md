# Ecosystem Repo Split And Parallel Dev

**Purpose:** Freeze the cross-repo ownership rules that let `styio-nightly`, `styio-spio`, and `styio-view` develop in parallel without source-level coupling or undocumented API drift.

**Last updated:** 2026-04-21

## Fixed Repo Split

1. `styio-nightly` owns compiler, language, machine-info, and managed-toolchain SSOT.
2. `styio-spio` owns package management, registry/backend services, hosted control-plane APIs, and the repo-hosted control console frontend.
3. `styio-view` owns the user-facing editor/runtime frontend and consumes backend/toolchain contracts through adapter boundaries.

## Contract Ownership

1. language, compiler, and managed-toolchain truth belongs to `styio-nightly`
2. package, project graph, dependency, deployment, and hosted workspace backend contracts belong to `styio-spio`
3. product-facing adapter contracts and UI-facing normalized result shapes belong to `styio-view`
4. cross-repo HTTP contracts ship from the owning repo as versioned interface packages, not as issue-thread prose or source-level conventions

## Contract Package Rule

1. HTTP APIs that cross repo boundaries must publish a versioned OpenAPI package.
2. Multi-step frontend/backend flows must publish a versioned Arazzo workflow package next to the API package.
3. Examples, lint config, and drift checks are part of the contract package; Markdown explains the package but does not replace it.
4. Consumer repos bind to the published package version and their own consumer-side mapping docs, not to private directory layout.

## Parallel Development Rules

1. Frontend teams develop against published contracts and examples, not upstream source layout.
2. Backend teams may change implementation internals freely as long as published contracts and examples remain compatible.
3. Breaking route, payload, or field changes require versioned contract updates; they do not happen in place under the same version.
4. If an upstream capability is not yet published, downstream repos must surface `blocked` or `partial` instead of inventing hidden heuristics.
5. Repo-local planning documents may explain sequencing, but they must not override the owning repo's SSOT for contracts or semantics.

## Non-Negotiable Boundaries

1. `styio-spio` must not depend on `styio-nightly` implementation headers or libraries.
2. `styio-view` must not infer backend truth from `.spio`, compiler install layout, or other private filesystem structures when a published payload exists.
3. `styio-nightly` does not own hosted control-plane or repo-console product behavior.
4. `styio-view` does not own package-manager or registry backend semantics.

## Delivery Checkpoint Rule

Any cross-repo change that moves responsibility, adds a new machine contract, or changes the supported handoff path must update:

1. the owning repo's SSOT
2. the affected consumer repo handoff document
3. the repository map in `styio-nightly`
