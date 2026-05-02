# Styio IDE Testing

**Purpose:** Provide the repeatable verification commands for the IDE-facing build, syntax backend, LSP server, and documentation inventory.

**Last updated:** 2026-04-16

## Recommended Commands

```bash
cmake -S . -B build/default -DSTYIO_ENABLE_TREE_SITTER=ON
cmake --build build/default --target styio_lspd styio_ide_test -j4
ctest --test-dir build/default --output-on-failure -L 'ide|lsp'
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
bash scripts/ide-quality-gate.sh --skip-fuzz --waiver 'skip fuzz on machines without libFuzzer'
```

## Useful Focused Runs

1. Syntax/LSP only: `ctest --test-dir build/default --output-on-failure -L 'ide|lsp'`
2. One IDE unit test: `ctest --test-dir build/default --output-on-failure -R 'StyioSyntaxParser.UsesTreeSitterBackendWhenAvailable'`
3. Incremental syntax reuse: `ctest --test-dir build/default --output-on-failure -R 'StyioSyntaxParser.ReusesIncrementalTreeForSubsequentParses'`
4. Recovery-mode semantic bridge: `ctest --test-dir build/default --output-on-failure -R 'StyioSemanticBridge.RecoversNightlyParseForLaterStatements'`
5. Service integration test: `ctest --test-dir build/default --output-on-failure -R 'StyioIdeService.DocumentSymbolsHoverDefinitionAndCompletion'`
6. Semantic query cache tests: `ctest --test-dir build/default --output-on-failure -R 'StyioSemanticDb'`
7. Stable HIR identity tests: `ctest --test-dir build/default --output-on-failure -R 'StyioHirBuilder'`
8. Type inference query tests: `ctest --test-dir build/default --output-on-failure -R 'StyioTypeInference|ExposesReceiverTypesForMembers|ExposesCallSiteExpectedTypes'`
9. Completion engine tests: `ctest --test-dir build/default --output-on-failure -R 'StyioCompletionEngine'`
10. Workspace index tests: `ctest --test-dir build/default --output-on-failure -R 'StyioWorkspaceIndex|DefinitionUsesWorkspaceIndexAcrossFiles|ReferencesMergeOpenFileAndBackgroundIndex'`
11. Runtime tests: `ctest --test-dir build/default --output-on-failure -R 'StyioLspRuntime'`
12. Syntax drift corpus: `ctest --test-dir build/default --output-on-failure -R 'StyioSyntaxDrift.CorpusMatchesApprovedEnvelope'`
13. Release perf gate: `bash scripts/ide-perf-gate.sh`
14. Fuzz smoke gate: `bash scripts/ide-fuzz-gate.sh`

## Expected Outcomes

1. `styio_lspd` builds and starts without missing symbol errors.
2. `StyioSyntaxParser.UsesTreeSitterBackendWhenAvailable` passes when Tree-sitter is enabled.
3. IDE/LSP tests keep passing when the grammar or syntax adapter changes.
4. `StyioSyntaxDrift.CorpusMatchesApprovedEnvelope` keeps the approved parser drift envelope explicit.
5. `StyioIdePerf.EnforcesFrozenLatencyBudgets` is enforced through the dedicated Release perf gate, while Debug IDE/LSP regression runs may skip it.
6. `docs-index.py --check` and `docs-audit.py` stay green after doc changes.
