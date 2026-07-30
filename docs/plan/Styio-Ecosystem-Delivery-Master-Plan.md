# Styio Ecosystem Delivery Master Plan

**Purpose:** Coordinate the finished product boundary among Styio, Pafio, Styio Platform, and Vityo through fixed revisions and one acceptance matrix.

**Last updated:** 2026-07-30

**Plan status:** Product ownership is frozen; coordinated fixed-revision
acceptance is the only remaining closure.

## 前置条件

1. 并行: repository-local implementation and focused tests may run in parallel.
   Public contract changes and the final revision matrix are serial merge gates.
2. 子智能体: sub-agents may be used only when explicitly requested and must own disjoint
   repositories or read-only evidence lanes.
3. 基座: common workflow, test, or documentation substrate changes must land through
   [Styio-Common-Foundation-Plan.md](./Styio-Common-Foundation-Plan.md) before
   product-specific consumers depend on them.

## 1. Frozen Product Model

Pafio is the Cargo/CMake-style terminal entry for Styio projects. It owns
project creation, manifests, locks, deterministic resolution, dependency sync,
metadata, project workflows, vendoring, packaging, and publishing.

Styio is a system-provided compiler. It owns compile-plan consumption,
compilation, diagnostics, receipts, runtime events, and language-service facts.
Pafio discovers it but never installs, updates, switches, pins, builds, or
caches it.

Styio Platform owns registry hosting and control, hosted workspaces, cloud
jobs, and workers. Workers call `pafio build`.

Vityo consumes Pafio metadata/workflow JSON, Styio compiler/language-service
contracts, and Platform hosted APIs through separate adapters.

## 2. Delivery Closures

| Closure | Exit evidence |
|---------|---------------|
| Pafio product | Retained command surface, automatic sync, `metadata v1`, external Styio discovery, package lifecycle, and focused tests |
| Styio consumer | Compile-plan producer identity `pafio`, compiler-owned result contracts, and interoperability tests |
| Platform owner | Pafio registry identifiers, registry-control route, hosted API, and worker `pafio build` |
| Vityo adapters | Separate Pafio metadata, Styio compiler, and Platform hosted adapters; no private Pafio storage access |
| Public entry points | Site, aggregate workspace, and independent audit all describe the same ownership model |
| Brand cutover | Public names use only Pafio identifiers; no runtime alias, old-data migration, or compatibility shim remains |

## 3. Release Sequence

1. Complete and test each repository-local closure.
2. Record an immutable commit for every participating repository.
3. Run each affected repository's full regression once.
4. Run one cross-repository acceptance matrix against exactly those revisions.
5. Update public site and release entry points only after the matrix succeeds.

The matrix is evidence, not a second implementation. Every schema and behavior
continues to be owned and tested in its source repository.

## 4. Non-goals

This plan does not add compiler distribution channels, compiler management to
Pafio, registry servers to Pafio, cloud or hosted execution to Pafio, private
IDE payloads to `metadata v1`, or duplicate Platform business logic in clients.

## 验收条件

1. Repository-local focused tests pass before full regression.
2. Each affected repository runs one full regression after implementation is
   complete.
3. The fixed-revision ecosystem matrix proves Pafio build, Platform
   publish/consume and worker execution, and Vityo local/hosted adapters.
4. Documentation and Better Plan validation report zero issues.
5. The one-time tracked-file migration search finds no retired product name or
   identifier; the temporary search is not retained as a permanent gate.
