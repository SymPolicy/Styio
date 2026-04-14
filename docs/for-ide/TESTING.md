# Styio IDE Testing

**Purpose:** Provide the repeatable verification commands for the IDE-facing build, syntax backend, LSP server, and documentation inventory.

**Last updated:** 2026-04-14

## Recommended Commands

```bash
cmake -S . -B build-codex -DSTYIO_ENABLE_TREE_SITTER=ON
cmake --build build-codex --target styio_lspd styio_ide_test -j4
ctest --test-dir build-codex --output-on-failure -L 'ide|lsp'
python3 scripts/docs-index.py --write
python3 scripts/docs-audit.py
```

## Useful Focused Runs

1. Syntax/LSP only: `ctest --test-dir build-codex --output-on-failure -L 'ide|lsp'`
2. One IDE unit test: `ctest --test-dir build-codex --output-on-failure -R 'StyioSyntaxParser.UsesTreeSitterBackendWhenAvailable'`
3. Service integration test: `ctest --test-dir build-codex --output-on-failure -R 'StyioIdeService.DocumentSymbolsHoverDefinitionAndCompletion'`

## Expected Outcomes

1. `styio_lspd` builds and starts without missing symbol errors.
2. `StyioSyntaxParser.UsesTreeSitterBackendWhenAvailable` passes when Tree-sitter is enabled.
3. IDE/LSP tests keep passing when the grammar or syntax adapter changes.
4. `docs-index.py --check` and `docs-audit.py` stay green after doc changes.
