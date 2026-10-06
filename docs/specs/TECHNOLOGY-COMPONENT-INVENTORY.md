# Technology And Component Inventory

**Purpose:** Define the required technology-stack, internal-component, open-source-component, and dependency-manifest inventory for `styio` / `styio-nightly`.

**Last updated:** 2026-04-24

This document is the repository-local maintenance rule for the manifest inventory audited by `General-Auditor`. The canonical audit module must list the same surfaces in `for-styio/module.json`; if this document and the audit manifest diverge, the change is not closed.

## Required Inventory Fields

Every audit manifest for this repository must maintain these non-empty lists:

1. `technology_stack`
2. `internal_components`
3. `open_source_components`
4. `dependency_manifests`

Missing or stale lists are audit failures. They block license, commercial-risk, ownership, and usage-boundary review because auditors cannot prove what stack and components are in scope.

## Current Inventory

Technology stack:

- C++ compiler/runtime code built with CMake and CTest.
- Styio source corpus and `.styio` fixtures.
- Tree-sitter grammar and parser shadow tooling.
- Python and Bash repository gates, docs automation, fuzz and benchmark scripts.
- GitHub Actions workflow automation.
- JavaScript, TypeScript, Zig, Nix, Bazel, and other fixture/tooling surfaces present in the repository.

Internal components:

- `StyioAST`, `StyioSema`, `StyioLowering`, `StyioIR`, and `StyioCodeGen` compiler pipeline.
- `StyioRuntime`, `StyioExtern`, and `StyioNative` runtime, built-in extern helper, and C/C++ native interop layers.
- `StyioIDE` and `StyioLSP` editor-facing workspace services.
- `StyioParser` parser route and shadow comparison pipeline.
- Security, pipeline, fuzz, benchmark, and checkpoint gate suites.
- Docs, workflow, and repo hygiene automation.

Open-source and external components:

- CMake and CTest.
- LLVM toolchain integration where present.
- Tree-sitter grammar tooling.
- GitHub Actions.
- Python standard library tooling.
- Bash shell tooling.
- Fixture-only JavaScript, TypeScript, Zig, Nix, Bazel, and other ecosystems requiring per-manifest evidence when promoted.

Dependency manifest surfaces:

- `CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `benchmark/CMakeLists.txt`, `tests/fuzz/CMakeLists.txt`.
- `Cargo.toml` files.
- `package.json` files.
- `.github/workflows/*.yml`.
- `.devcontainer/devcontainer.json`.
- `.docker/docker-compose.yaml`.

## Maintenance Rule

Update this document and the matching `General-Auditor` project module in the same change whenever any of these occur:

1. A language, SDK, runtime, build system, CI system, package manager, or generated-code tool is added or removed.
2. A first-party component, module, service, parser route, runtime surface, IDE/LSP surface, gate, or workflow boundary is added, renamed, or retired.
3. An open-source or external component is introduced, removed, vendored, promoted from fixture-only to production use, or given a new usage boundary.
4. A dependency manifest is added, removed, renamed, or moved.
5. License, Apache-2.0, commercial-authorization, subscription, membership, trial-only, or proprietary-use evidence changes.

For new external dependencies, update [THIRD-PARTY.md](./THIRD-PARTY.md) and this inventory together before the change can pass audit.
