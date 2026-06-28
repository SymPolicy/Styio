# Nightly IDE / Parser Audit Shard

**Purpose:** Record IDE, LSP, parser, cache, and protocol-boundary findings from the parallel external audit pass.

**Last updated:** 2026-06-28

**Date:** 2026-04-22
**Repo:** `styio-nightly`
**Scope:** IDE/LSP/parser lifecycle, cache and index invalidation, protocol input boundaries, and gate/test coverage.

> **MIGRATION-NEEDED: M-AUDIT-01** (docs/rollups/MIGRATION-LEDGER.md). This dated audit shard is older than the audit-retention window per `docs/audit/README.md`. Verify each finding is closed against `docs/adr/IMPLEMENTED-DECISIONS.md` / `docs/rollups/IM-D*` inventories, archive through `docs/archive/ARCHIVE-MANIFEST.json`, and remove this file from the current tree.

## Findings

| ID | Area | Status | Evidence | Notes |
|----|------|--------|----------|-------|
| NIP-001 | IDE lifecycle | Resolved | `src/StyioIDE/VFS.cpp:161-177` | Closed-file snapshots now re-read disk contents on each access, so background indexing no longer keeps stale in-memory text after a file changes on disk. |
| NIP-002 | Cache / index | Resolved | `src/StyioIDE/SemDB.cpp:636-653` | Workspace indexing now persists an empty symbol set as well, so a later session clears deleted symbols instead of resurrecting them from an old `symbols.json`. |
| NIP-003 | LSP protocol boundary | Resolved | `src/StyioLSP/Server.cpp:13-22`, `src/StyioLSP/Server.cpp:247-322` | LSP frame parsing now validates `Content-Length`, caps frame size, discards oversized frames safely, and ignores oversized string request IDs without throwing. |
| NIP-004 | Workspace scan / gate | Resolved | `src/StyioServices/StyioIDE/Project.cpp`, `tests/ide/styio_ide_test.cpp` | Recursive workspace scanning now uses `std::filesystem` error-code traversal with `workspace_scan_error_count()`, and cache-root identity uses stable `root-<hex-path>` values instead of process-local `std::hash<std::string>`. |

## Regression Coverage Added

- `StyioWorkspaceIndex.PersistentIndexClearsDeletedSymbolsOnNewSession`
- `StyioWorkspaceIndex.ClosedFileRefreshesFromDiskBeforeBackgroundIndexing`
- `StyioLspServer.SkipsMalformedFramesAndHandlesLargeStringIds`
- `StyioIdeProject.EnvironmentFallbacksAndWorkspaceSkipsStayExplicit`

## Validation

- `cmake --build <workspace-root>/build-codex --target styio_ide_test styio_lspd -j8`
- `ctest --test-dir <workspace-root>/build-codex -R 'StyioWorkspaceIndex\\.(PersistentIndexClearsDeletedSymbolsOnNewSession|ClosedFileRefreshesFromDiskBeforeBackgroundIndexing)|StyioLspServer\\.(HandlesInitializeOpenAndCompletion|SkipsMalformedFramesAndHandlesLargeStringIds|RunDrainsRuntimeDiagnostics)' --output-on-failure`
- `ctest --test-dir <workspace-root>/build-codex -R 'StyioLspRuntime\\.(RunAdvancesBackgroundWorkAsRequestDrivenFallback|BackgroundIndexYieldsToForegroundRequests|IdleSliceDrainsSemanticBeforeBackgroundWork|CancellationPropagatesThroughSemanticQueries|DebouncesSemanticDiagnostics)' --output-on-failure`

## Residual Risk

- The parser-shadow scripts were reviewed as part of the audit surface, but no parser-shadow-specific code path needed a local fix in this pass.
