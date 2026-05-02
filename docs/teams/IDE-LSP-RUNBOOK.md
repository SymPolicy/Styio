# IDE / LSP Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of `styio_ide_core`, `styio_lspd`, IDE-facing C++ APIs, VFS snapshots, syntax/HIR/SemDB services, and LSP protocol behavior.

**Last updated:** 2026-04-30

## Mission

Own edit-time developer experience and host integration. This team makes compiler facts usable through IDE APIs and LSP without redefining language semantics. The Nightly parser/analyzer remains semantic truth; IDE syntax layers provide recovery and interaction support.

## Owned Surface

Primary paths:

1. `src/StyioIDE/`
2. `src/StyioLSP/`
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
10. Preserve the M18 runtime scheduling contract: request-loop drains are budgeted, foreground work yields over queued background reindexing, and explicit idle slices drain semantic diagnostics before background work.
11. Mirror lexer token additions in the tolerant syntax layer so edit-time diagnostics and grouping do not drift from compiler tokenization.
12. When testing `VFS` close/drop-open-file behavior, put expected closed-file contents on disk before closing the in-memory document; closed snapshots intentionally reload from disk instead of retaining stale open-buffer query state.
13. Keep compiler bridge code pointed at `AstToStyioIRLowerer` for semantic truth; do not rebuild a separate IDE analyzer or depend on the legacy `StyioAnalyzer` compatibility alias for new code.

## Change Classes

1. Small: completion ranking, DTO cleanup, or local VFS/Syntax helper fix. Run IDE unit tests.
2. Medium: public C++ API, incremental edit application, HIR identity, SemDB cache, or LSP method behavior. Update tests and `docs/external/for-ide/`.
3. High: document sync contract, semantic cache model, workspace index behavior, or LSP surface expansion. Use checkpoint workflow and coordinate docs plus tests.

## Required Gates

Minimum local commands:

```bash
cmake --build build/default --target styio_lspd styio_ide_test
ctest --test-dir build/default -L ide
```

When syntax backend behavior changes:

```bash
ctest --test-dir build/default -L ide --output-on-failure
python3 scripts/docs-audit.py
```

When runtime scheduling or LSP drain behavior changes:

```bash
ctest --test-dir build/default -L ide --tests-regex 'StyioLspRuntime|StyioLspServer.RunDrainsRuntimeDiagnostics' --output-on-failure
python3 scripts/docs-audit.py
```

## Cross-Team Dependencies

1. Grammar must review tree-sitter or edit-time CST changes.
2. Frontend and Sema / IR must review changes that depend on parser/analyzer truth.
3. Test Quality must review new IDE regression tests.
4. Docs / Ecosystem must review host-facing documentation changes.

## Handoff / Recovery

Record unfinished IDE/LSP work with:

1. Affected layer: VFS, Syntax, HIR, SemDB, Service, or LSP.
2. Public method or protocol message involved.
3. Repro document text and cursor/range.
4. Expected diagnostics/completion/hover/symbol behavior.
5. Exact `ctest -L ide` failure or manual LSP message flow.
