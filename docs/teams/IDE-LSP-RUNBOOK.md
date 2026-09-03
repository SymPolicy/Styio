# IDE / LSP Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of `styio_ide_core`, `styio_lspd`, IDE-facing C++ APIs, VFS snapshots, syntax/HIR/SemDB services, and LSP protocol behavior.

**Last updated:** 2026-09-04

## Mission

Own edit-time developer experience and host integration. This team makes compiler facts usable through IDE APIs and LSP without redefining language semantics. The Nightly parser/analyzer remains semantic truth; IDE syntax layers provide recovery and interaction support.

## Owned Surface

Primary paths:

1. `src/StyioServices/StyioIDE/`
2. `src/StyioServices/StyioLSP/`
3. `docs/external/for-ide/`
4. `tests/ide/styio_ide_test.cpp`

Build and test targets:

1. `styio_ide_core`
2. `styio_lspd`
3. `styio_ide_test`

## Daily Workflow

1. Start from [../external/for-ide/README.md](../external/for-ide/README.md), then the relevant `BUILD`, `CXX-API`, `LSP`, `TREE-SITTER`, or `TESTING` page.
2. Decide whether the change is syntax-only, semantic bridge, HIR/SemDB, service API, or LSP boundary.
3. Keep UTF-16 LSP positions at the server boundary and UTF-8 byte offsets inside IDE core.
4. Preserve recovery behavior: malformed statements should not unnecessarily erase later useful IDE facts.
5. Keep `docs/external/for-ide/BUILD.md` scoped to IDE/LSP targets; repository-wide bootstrap and common compiler commands belong in [../BUILD-AND-DEV-ENV.md](../BUILD-AND-DEV-ENV.md).
6. Update `docs/external/for-ide/` when public host behavior changes.
7. When IDE build docs mention compiler prerequisites, reflect the shared repository baseline instead of creating a second LLVM/CMake/Python version matrix under `docs/external/for-ide/`.
8. Tree-sitter maintenance instructions in `docs/external/for-ide/BUILD.md` must keep using the repository-standard Node.js `v24.15.0` LTS line instead of a floating `stable` or distro-default Node release.
9. Keep builtin/default-symbol completions sourced from the shared compiler-owned symbol registry under `src/StyioParser/`; do not reintroduce a private IDE-only builtin or keyword table.
10. Preserve the runtime scheduling contract: request-loop drains are budgeted, foreground work yields over queued background reindexing, and explicit idle slices drain semantic diagnostics before background work.
11. Mirror lexer token additions in the tolerant syntax layer so edit-time diagnostics and grouping do not drift from compiler tokenization.
12. When async, continuation, or task syntax adds tokens such as `?|` or `||>`, update `src/StyioIDE/Syntax.cpp` in the same change so tolerant highlighting and diagnostics recognize the new token boundary.
13. When testing `VFS` close/drop-open-file behavior, put expected closed-file contents on disk before closing the in-memory document; closed snapshots intentionally reload from disk instead of retaining stale open-buffer query state.
14. Keep compiler bridge code pointed at `AstToStyioIRLowerer` for semantic truth; do not rebuild a separate IDE analyzer or depend on the legacy `StyioAnalyzer` compatibility alias for new code.
15. Keep LSP lifecycle and transport behavior byte-exact: notifications such as `initialized` must not receive JSON-RPC responses, and `styio_lspd` must keep stdio in binary mode on Windows before any LSP frame is exchanged.
16. Keep IDE/LSP tests on the current public include roots, such as `StyioServices/StyioIDE/` and `StyioServices/StyioLSP/`; do not preserve old short include paths after source directories move.
17. Tolerant IDE tokenization should keep full multi-character continue lexemes such as `>>>` together for editor spans, but it must not imply a different compiler semantic depth than the parser-owned nearest-loop continue.
18. Native macOS IDE/LSP builds must use the repository-level LLVM 18.1.x prefix and explicitly resolved macOS SDK instead of architecture-specific package paths. The macOS compatibility lane must build the real `styio_lspd` and fail if the byte-level stdio framing test is not registered.
19. Treat a macOS compatibility build configured with `STYIO_ENABLE_TREE_SITTER=OFF` as offline compiler/LSP host evidence only; Tree-sitter-enabled syntax behavior still requires the focused IDE test path.
20. Watched-file refreshes must accept only concrete closed `.styio` paths inside the selected workspace, coalesce duplicates, treat an empty change list as a no-op, and preserve an empty background-index tombstone for deleted files so stale persistent symbols cannot reappear.
21. Keep edit snapshots immutable and cheap to copy. `TextBuffer` copies share read-only text plus line-index storage until `reset` installs a fresh snapshot; `SyntaxParser` retains that buffer in its incremental cache instead of duplicating the source string. Tolerant tokenization must construct final ranged `SyntaxToken` values directly rather than allocating a second whole-file token representation. Preserve byte ranges, diagnostics, Tree-sitter reuse, and full/incremental token equivalence while keeping `StyioIdePerf.EnforcesFrozenLatencyBudgets` green.
22. Resource-topology failures remain compiler-owned Sema diagnostics. The IDE semantic bridge must forward the unchanged `sema-resource-topology` message under the existing type phase, without exposing AST pointers, machine paths, or a separate IDE topology analyzer.

## Change Classes

1. Small: completion ranking, DTO cleanup, or local VFS/Syntax helper fix. Run IDE unit tests.
2. Medium: public C++ API, incremental edit application, HIR identity, SemDB cache, or LSP method behavior. Update tests and `docs/external/for-ide/`.
3. High: document sync contract, semantic cache model, workspace index behavior, or LSP surface expansion. Use checkpoint workflow and coordinate docs plus tests.

## Required Gates

Minimum local commands:

```bash
cmake --build build/default --target styio_lspd styio_ide_test
ctest --test-dir build/default -L ide --output-on-failure --no-tests=error
```

When syntax backend behavior changes:

```bash
ctest --test-dir build/default -L ide --output-on-failure --no-tests=error
python3 scripts/docs-audit.py
```

When runtime scheduling or LSP drain behavior changes:

```bash
ctest --test-dir build/default -L ide --tests-regex 'StyioLspRuntime|StyioLspServer.RunDrainsRuntimeDiagnostics' --output-on-failure --no-tests=error
python3 scripts/docs-audit.py
```

When LSP transport startup changes:

```bash
cmake --build build/default --target styio_lspd
ctest --test-dir build/default -R '^styio_lspd_stdio_framing$' --output-on-failure --no-tests=error
python3 scripts/docs-audit.py
```

When workspace indexing or watched-file handling changes:

```bash
ctest --test-dir build/default -R '^(StyioWorkspaceIndex\.(PersistentIndexClearsDeletedSymbolsOnNewSession|ClosedFileRefreshesFromDiskBeforeBackgroundIndexing)|StyioIdeProject\.EnvironmentFallbacksAndWorkspaceSkipsStayExplicit|StyioIdeService\.WatchFileRefreshFiltersAndCoalescesChangedPaths|StyioLspServer\.WatchFileChangesFilterAndCoalesceBackgroundRefresh)$' --output-on-failure --no-tests=error
```

Native macOS compatibility after the repository-level configure:

```bash
cmake --build build/macos --parallel --target styio_lspd styio_platform_internal_test
ctest --test-dir build/macos -R '^styio_platform_internal_test$' --output-on-failure --no-tests=error
ctest --test-dir build/macos -R '^styio_lspd_stdio_framing$' --output-on-failure --no-tests=error
```

## Cross-Team Dependencies

1. Grammar must review tree-sitter or edit-time CST changes.
2. Frontend and Sema / IR must review changes that depend on parser/analyzer truth.
3. Test Quality must review new IDE regression tests.
4. Docs / Ecosystem must review host-facing documentation changes.
5. Range CST changes must preserve the IDE/compiler boundary: Tree-sitter may expose `range_expr` and `materialized_range` nodes, but active syntax acceptance and reserved step-range rejection remain owned by the nightly compiler parser.

## Handoff / Recovery

Record unfinished IDE/LSP work with:

1. Affected layer: VFS, Syntax, HIR, SemDB, Service, or LSP.
2. Public method or protocol message involved.
3. Repro document text and cursor/range.
4. Expected diagnostics/completion/hover/symbol behavior.
5. Exact `ctest -L ide` failure or manual LSP message flow.
