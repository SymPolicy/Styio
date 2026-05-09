# For IDE Docs

**Purpose:** Define the scope and naming rules for `docs/external/for-ide/`; this directory is the SSOT for consuming `styio`'s IDE-facing components, while the generated inventory lives in [INDEX.md](./INDEX.md).

**Last updated:** 2026-04-19

## Scope

1. Document how to build, run, embed, and verify `styio_ide_core` and `styio_lspd`.
2. Keep edit-time syntax backend guidance here, including Tree-sitter grammar maintenance.
3. Do not restate language semantics here; semantic SSOT remains in [../../design/INDEX.md](../../design/INDEX.md).
4. IDE-facing tradeoffs still follow [../../specs/PRINCIPLES-AND-OBJECTIVES.md](../../specs/PRINCIPLES-AND-OBJECTIVES.md).
5. Repository-wide bootstrap and common build commands live in [../../BUILD-AND-DEV-ENV.md](../../BUILD-AND-DEV-ENV.md), not in this directory.

## Naming Rules

1. Use task_shaped filenames such as `BUILD.md`, `LSP.md`, `CXX-API.md`, and `TESTING.md`.
2. Keep one document per integration path rather than mixing build, protocol, and grammar topics.
3. When a workflow changes, update the owning page here and regenerate [INDEX.md](./INDEX.md).

## Recommended Entry Points

1. Build and run: [BUILD.md](./BUILD.md)
2. Editor protocol integration: [LSP.md](./LSP.md)
3. Direct C++ embedding: [CXX-API.md](./CXX-API.md)
4. Grammar and syntax backend maintenance: [TREE-SITTER.md](./TREE-SITTER.md)
5. Verification commands: [TESTING.md](./TESTING.md)
6. Repository bootstrap entry: [../../BUILD-AND-DEV-ENV.md](../../BUILD-AND-DEV-ENV.md)

## Inventory

See [INDEX.md](./INDEX.md).
