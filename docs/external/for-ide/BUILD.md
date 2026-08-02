# Styio IDE Build Guide

**Purpose:** Describe how to configure, build, and launch the IDE-facing targets `styio_ide_core` and `styio_lspd`, including the optional Tree-sitter syntax backend, after the repository-level toolchain is already in place.

**Last updated:** 2026-07-28

## Targets

1. `styio_frontend_core`: tokenizer, Nightly LL parser, AST, analyzer, and semantic bridge.
2. `styio_ide_core`: VFS, incremental syntax snapshot, HIR, semdb, index, diagnostics, and IDE DTOs.
3. `styio_lspd`: stdio-based LSP 3.17 server for editor integration.

## Prerequisites

For repository-level bootstrap, common compiler commands, and docs tooling, start with [../../BUILD-AND-DEV-ENV.md](../../BUILD-AND-DEV-ENV.md).

1. LLVM `18.1.x` discoverable by `find_package(LLVM ...)`; `18.1.0` is the compatibility floor and `18.1.x` is the standardized line.
2. A C++20 compiler and CMake / CTest `3.31.6` on the standardized toolchain; repository compatibility floor is CMake `3.20`.
3. Node.js `v24.15.0` LTS when regenerating the Tree-sitter grammar.

For a fresh Debian/Ubuntu container, VM, or macOS host, bootstrap the toolchain first:

```bash
./scripts/bootstrap-dev-env.sh
```

On macOS, install Xcode Command Line Tools and Homebrew first. The bootstrap
selects the versioned Homebrew LLVM, ICU, Python, and Node formulae; follow the
native configure recipe in [../../BUILD-AND-DEV-ENV.md](../../BUILD-AND-DEV-ENV.md)
so Homebrew Clang receives the active macOS SDK explicitly.

## Configure

```bash
cmake -S . -B build/default \
  -DSTYIO_ENABLE_TREE_SITTER=ON \
  -DSTYIO_USE_ICU=OFF
```

`STYIO_ENABLE_TREE_SITTER=ON` is the default. Set it to `OFF` if you need the repository-local edit-time snapshot backend with no FetchContent network step. Both modes are editor-support modes; syntax validity is still owned by the hand-written nightly compiler parser.

## Build

```bash
cmake --build build/default --parallel 4 --target styio_lspd styio_ide_test
```

Useful target sets:

1. Runtime only: `cmake --build build/default --parallel 4 --target styio_lspd`
2. Full compiler + IDE: `cmake --build build/default --parallel 4 --target styio styio_lspd`
3. IDE tests only: `cmake --build build/default --parallel 4 --target styio_ide_test`

## Prune Old Build Directories

Preview which build directories would be removed while keeping the newest three:

```bash
scripts/cleanup-builds.sh
```

Apply the cleanup:

```bash
scripts/cleanup-builds.sh --apply
```

The script only touches repo-root directories named `build` or `build-*`, and sorts them by modification time.

## Run The LSP Daemon

```bash
./build/default/bin/styio_lspd
```

The server uses stdio JSON-RPC. Your IDE host is expected to launch it as a long-lived local process.

## Current Parsing Behavior

1. Edit-time syntax goes through Tree-sitter when `STYIO_ENABLE_TREE_SITTER=ON`.
2. `SyntaxParser` reuses the previous Tree-sitter tree for the same file path and applies a single incremental edit before reparsing.
3. Semantic analysis uses the authoritative nightly compiler parser in strict mode. Malformed source emits diagnostics and does not publish recovered later semantic facts.

## Regenerate The Grammar

```bash
cd grammar/tree-sitter-styio
npx --yes tree-sitter-cli@0.26.8 generate
```

This updates:

1. `grammar/tree-sitter-styio/src/parser.c`
2. `grammar/tree-sitter-styio/src/grammar.json`
3. `grammar/tree-sitter-styio/src/node-types.json`

After regeneration, rebuild `styio_ide_core` or `styio_lspd`.
