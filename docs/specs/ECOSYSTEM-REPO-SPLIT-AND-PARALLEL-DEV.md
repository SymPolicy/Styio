# Ecosystem Repo Split And Parallel Dev

**Purpose:** Freeze the cross-repo ownership rules that let Styio, Pafio, Styio Platform, and Vityo develop in parallel without source-level coupling or undocumented API drift.

**Last updated:** 2026-07-30

## Fixed Repo Split

1. `styio-nightly` owns language, compiler, machine-info, diagnostics, receipts, runtime events, and language-service facts.
2. `pafio-nightly` owns project manifests, locks, resolution, metadata, project workflows, vendor, pack, and the publish client.
3. Styio Platform owns registry service/control, hosted workspace, cloud job, and worker behavior.
4. `vityo-nightly` owns the user-facing editor/runtime frontend and consumes Pafio, Styio, and Platform through separate adapters.

## Contract Ownership

1. language and compiler truth belongs to `styio-nightly`
2. project/package state and local workflow envelopes belong to `pafio-nightly`
3. registry and hosted/cloud service contracts belong to Styio Platform
4. product-facing adapter contracts and UI-facing normalized result shapes belong to `vityo-nightly`
5. cross-repo HTTP contracts ship from the owning service repository with explicit schema markers, not as issue-thread prose or source-level conventions

## Contract Package Rule

1. HTTP APIs that cross repo boundaries must publish a named OpenAPI package with an explicit schema marker.
2. Multi-step frontend/backend flows must publish a named Arazzo workflow package next to the API package.
3. Examples, lint config, and drift checks are part of the contract package; Markdown explains the package but does not replace it.
4. Consumer repos bind to the published package identity, schema marker, and their own consumer-side mapping docs, not to private directory layout.

## Parallel Development Rules

1. Frontend teams develop against published contracts and examples, not upstream source layout.
2. Backend teams may change implementation internals freely as long as published contracts and examples remain compatible.
3. Breaking route, payload, or field changes require explicit contract-shape updates with migration notes; they do not happen silently in place.
4. If an upstream capability is not yet published, downstream repos must surface `blocked` or `partial` instead of inventing hidden heuristics.
5. Repo-local planning documents may explain sequencing, but they must not override the owning repo's SSOT for contracts or semantics.

## Non-Negotiable Boundaries

1. `pafio-nightly` must not depend on `styio-nightly` implementation headers or libraries.
2. `vityo-nightly` must not infer backend truth from `.pafio`, compiler install layout, or other private filesystem structures when a published payload exists.
3. `styio-nightly` does not own hosted control-plane or repo-console product behavior.
4. `pafio-nightly` does not host registry, cloud, worker, or console services.
5. `vityo-nightly` does not own package-manager or registry backend semantics.

## Delivery Checkpoint Rule

Any cross-repo change that moves responsibility, adds a new machine contract, or changes the supported handoff path must update:

1. the owning repo's SSOT
2. the affected consumer repo handoff document
3. the repository map in `styio-nightly`
