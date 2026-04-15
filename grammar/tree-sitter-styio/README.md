# tree-sitter-styio

**Purpose:** Describe the repository-local Tree-sitter grammar used by `styio_ide_core` for edit-time CST construction, error recovery, and IDE-facing syntax services.

**Last updated:** 2026-04-14

This directory is the repository-local home for Styio's edit-time syntax grammar.

The current IDE implementation uses this generated grammar together with the tolerant token layer
in `src/StyioIDE/Syntax.cpp`. Tree-sitter is responsible for CST nodes, error-node discovery, and
folding-oriented structure; the tolerant token layer still provides token spans, bracket matching,
and the lightweight heuristics that feed completion and HIR fallback.

Current expectations:

- `grammar.js` is the source of truth for the edit-time grammar surface.
- `src/parser.c`, `src/grammar.json`, and `src/node-types.json` are generated artifacts; regenerate
  them with `npx --yes tree-sitter-cli@0.26.8 generate` after grammar changes.
- `src/StyioIDE/TreeSitterBackend.cpp` adapts Tree-sitter nodes into the repository-local
  `SyntaxSnapshot` API without exposing Tree-sitter types outside `styio_ide_core`.
- If a build must stay offline or avoid the dependency fetch, configure with
  `-DSTYIO_ENABLE_TREE_SITTER=OFF`; `Syntax.cpp` will fall back to the tolerant backend only.
