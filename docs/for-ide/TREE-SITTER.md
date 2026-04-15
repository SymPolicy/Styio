# Styio Tree-sitter Backend

**Purpose:** Explain how the repository-local Tree-sitter grammar plugs into `styio_ide_core`, and how to evolve it without breaking the IDE contracts.

**Last updated:** 2026-04-14

## Ownership

1. Grammar source: [../../grammar/tree-sitter-styio/grammar.js](../../grammar/tree-sitter-styio/grammar.js)
2. Generated sources: `grammar/tree-sitter-styio/src/parser.c`, `src/grammar.json`, `src/node-types.json`
3. Runtime adapter: [../../src/StyioIDE/TreeSitterBackend.cpp](../../src/StyioIDE/TreeSitterBackend.cpp)
4. Snapshot façade: [../../src/StyioIDE/Syntax.hpp](../../src/StyioIDE/Syntax.hpp)

## What Tree-sitter Owns Today

1. CST node labels and nesting for `SyntaxSnapshot::nodes`
2. Error-node detection and additional syntax diagnostics
3. Folding-oriented structural ranges
4. Incremental tree reuse for repeated parses of the same file path

## What It Does Not Own

1. Token spans used by HIR and semantic tokens
2. Bracket matching maps used by the fallback HIR builder
3. Nightly parser semantic truth
4. Completion ranking and type-aware filtering

## Update Workflow

1. Edit `grammar.js`.
2. Run `npx --yes tree-sitter-cli@0.26.8 generate` inside `grammar/tree-sitter-styio/`.
3. Rebuild `styio_ide_core` or `styio_lspd`.
4. Run [TESTING.md](./TESTING.md) before landing the change.

## Build Toggle

`STYIO_ENABLE_TREE_SITTER=ON` links the generated parser and upstream runtime into `styio_ide_core`.

`STYIO_ENABLE_TREE_SITTER=OFF` keeps the old tolerant backend as the only syntax source. This mode is valid for bring-up or offline builds, but it should not be the default path for IDE development.

## Incremental Reuse Contract

1. `SyntaxParser` caches the last successful Tree-sitter tree per document path.
2. On the next parse of the same path, the parser computes a single byte-range edit from the old text to the new text, applies `ts_tree_edit`, and reparses against that edited tree.
3. `SyntaxSnapshot::reused_incremental_tree` is the observable signal that the old tree was reused.
4. Closing a file through `SemanticDB::drop_open_file` drops the cached incremental tree for that path.
