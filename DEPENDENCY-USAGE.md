# Dependency Usage Boundary

**Purpose:** Record dependency authorization boundaries for `styio` / `styio-nightly`.

**Last updated:** 2026-04-24

`styio` / `styio-nightly` is an Apache-2.0 compiler/runtime source project. Its current build, test, docs, and fixture dependency boundary is:

- CMake and CTest drive native compiler/runtime build and test workflows.
- LLVM integration, `tree_sitter_runtime`, GitHub Actions, `Python3`, and Bash scripts are external build, parser, CI, and automation surfaces.
- `ICU` is an optional Unicode support dependency enabled through CMake configuration and must remain bounded to the documented Unicode/runtime surface.
- `googletest` is a test-only dependency and must remain outside runtime and shipped compiler artifacts.
- Rust, JavaScript, TypeScript, Zig, Nix, and Bazel surfaces are fixture/tooling surfaces unless separately promoted with explicit manifest evidence.
- Repository workflows may invoke system tools such as `git`, `cmake`, shell utilities, and configured compiler/runtime tools through explicit process boundaries.

Named manifest dependencies currently in scope:

- `ICU`: optional system package, license and usage boundary tracked in `docs/specs/THIRD-PARTY.md`; not permitted to introduce commercial authorization or proprietary runtime obligations.
- `Python3`: standard interpreter/tooling dependency for docs, audit, and maintenance automation; must remain open-source and locally executable without subscription or membership access.
- `googletest`: test-only upstream dependency fetched for validation; must not cross the usage boundary into runtime or distributed production components.
- `tree_sitter_runtime`: parser/IDE support dependency used for edit-time syntax tooling; must remain covered by open-source license evidence and explicit parser/IDE usage boundaries.

Dependency policy:

- No dependency may require commercial authorization, paid licensing, subscription access, membership access, trial-only terms, proprietary-use approval, or private registry access.
- Any future dependency must be listed here with its license evidence, source boundary, and usage boundary before it can pass audit.
- Fixture-only dependencies must stay fixture-scoped and must not become runtime requirements without this file being updated.
- Generated reports and gate summaries must summarize dependency and license evidence without copying target repository source.
