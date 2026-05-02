# Grammar Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of the repository-local Tree-sitter grammar and edit-time syntax backend contract.

**Last updated:** 2026-05-01

## Mission

Own edit-time CST structure, Tree-sitter error-node behavior, generated grammar artifacts, and the grammar-to-`SyntaxSnapshot` adapter contract. This team does not own compiler parser semantics or type truth.

## Owned Surface

Primary paths:

1. `grammar/tree-sitter-styio/grammar.js`
2. `grammar/tree-sitter-styio/src/parser.c`
3. `grammar/tree-sitter-styio/src/grammar.json`
4. `grammar/tree-sitter-styio/src/node-types.json`
5. `src/StyioIDE/TreeSitterBackend.*`
6. [../external/for-ide/TREE-SITTER.md](../external/for-ide/TREE-SITTER.md)

## Daily Workflow

1. Check [../external/for-ide/TREE-SITTER.md](../external/for-ide/TREE-SITTER.md) and `grammar/tree-sitter-styio/README.md`.
2. Edit `grammar.js` for grammar source changes.
3. Regenerate artifacts from `grammar/tree-sitter-styio/`.
4. Review generated diffs for suspicious broad churn.
5. Run IDE tests with `STYIO_ENABLE_TREE_SITTER=ON`; when relevant, also configure with it OFF to preserve tolerant fallback.
6. For token repetitions such as `^...`, keep edit-time recognition tolerant while following the language SSOT for semantics; caret count is not a Tree-sitter-owned break-depth contract.
7. When conditional-loop syntax changes, keep compiler parser tests, [../design/Styio-EBNF.md](../design/Styio-EBNF.md), language design docs, and symbol reference aligned; the active form is `[...] >> ?(condition) => { ... }`.

## Change Classes

1. Small: CST label adjustment or grammar conflict cleanup with unchanged IDE behavior. Run IDE tests.
2. Medium: new syntax form, changed error recovery, changed folding/node range, or generated artifact update. Update tests and docs.
3. High: SyntaxSnapshot contract, incremental tree reuse, or Tree-sitter build toggle behavior. Use checkpoint workflow and coordinate with IDE / LSP.

## Required Gates

Regenerate grammar:

```bash
cd grammar/tree-sitter-styio
npx --yes tree-sitter-cli@0.26.8 generate
```

Build and test:

```bash
cmake -S . -B build/default -DSTYIO_ENABLE_TREE_SITTER=ON
cmake --build build/default --target styio_ide_test styio_lspd
ctest --test-dir build/default -L ide
```

Fallback check when adapter behavior changes:

```bash
cmake -S . -B build-no-tree-sitter -DSTYIO_ENABLE_TREE_SITTER=OFF
cmake --build build-no-tree-sitter --target styio_ide_test
ctest --test-dir build-no-tree-sitter -L ide
```

## Cross-Team Dependencies

1. IDE / LSP must review every syntax snapshot or adapter behavior change.
2. Frontend must review grammar changes that claim alignment with compiler parser behavior.
3. Test Quality must review IDE regression coverage for error recovery and incremental reuse.
4. Docs / Ecosystem must review `docs/external/for-ide/` updates.

## Handoff / Recovery

Record unfinished grammar work with:

1. Grammar rule or node type touched.
2. Whether generated artifacts are up to date.
3. IDE behavior expected from the CST change.
4. Tree-sitter ON/OFF test status.
5. Known parser semantic mismatch, if any.
