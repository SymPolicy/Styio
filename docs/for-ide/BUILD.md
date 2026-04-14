# Styio IDE Build Guide

**Purpose:** Describe how to configure, build, and launch the IDE-facing targets `styio_ide_core` and `styio_lspd`, including the optional Tree-sitter syntax backend.

**Last updated:** 2026-04-14

## Targets

1. `styio_frontend_core`: tokenizer, Nightly LL parser, AST, analyzer, and semantic bridge.
2. `styio_ide_core`: VFS, syntax snapshot, HIR, semdb, index, diagnostics, and IDE DTOs.
3. `styio_lspd`: stdio-based LSP 3.17 server for editor integration.

## Prerequisites

1. LLVM `18.1.0` discoverable by `find_package(LLVM ...)`.
2. A C++20 compiler and CMake `>= 3.14`.
3. Node.js only when regenerating the Tree-sitter grammar.

## Configure

```bash
cmake -S . -B build-codex \
  -DSTYIO_ENABLE_TREE_SITTER=ON \
  -DSTYIO_USE_ICU=OFF
```

`STYIO_ENABLE_TREE_SITTER=ON` is the default. Set it to `OFF` if you need a pure tolerant syntax build with no FetchContent network step.

## Build

```bash
cmake --build build-codex --target styio_lspd styio_ide_test -j4
```

Useful target sets:

1. Runtime only: `cmake --build build-codex --target styio_lspd -j4`
2. Full compiler + IDE: `cmake --build build-codex --target styio styio_lspd -j4`
3. IDE tests only: `cmake --build build-codex --target styio_ide_test -j4`

## Run The LSP Daemon

```bash
./build-codex/bin/styio_lspd
```

The server uses stdio JSON-RPC. Your IDE host is expected to launch it as a long-lived local process.

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
