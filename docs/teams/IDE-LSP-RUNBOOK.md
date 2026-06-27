# IDE / LSP Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of `styio_ide_core`, `styio_lspd`, IDE-facing C++ APIs, VFS snapshots, syntax/HIR/SemDB services, and LSP protocol behavior.

**Last updated:** 2026-06-26

## Mission

Own edit-time developer experience and host integration. This team makes compiler facts usable through IDE APIs and LSP without redefining language semantics. The hand-written nightly compiler parser remains grammar and semantic truth; IDE syntax snapshots provide interaction support only.

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
4. Do not treat editor syntax snapshots as grammar authority. Malformed source may keep token/CST interaction hints, but compiler semantic facts must come from strict nightly parsing.
5. Keep `docs/external/for-ide/BUILD.md` scoped to IDE/LSP targets; repository-wide bootstrap and common compiler commands belong in [../BUILD-AND-DEV-ENV.md](../BUILD-AND-DEV-ENV.md).
6. Update `docs/external/for-ide/`, the relevant source-level module README, and `src/StyioServices/MANIFEST.md` when public host behavior changes.
7. When IDE build docs mention compiler prerequisites, reflect the shared repository baseline instead of creating a second LLVM/CMake/Python toolchain baseline table under `docs/external/for-ide/`.
8. Tree-sitter maintenance instructions in `docs/external/for-ide/BUILD.md` must keep using the repository-standard Node.js `v24.15.0` LTS line instead of a floating `stable` or distro-default Node release.
9. Keep builtin/default-symbol completions sourced from the shared compiler-owned symbol registry under `src/StyioParser/`; do not reintroduce a private IDE-only builtin or keyword table.
10. Preserve the runtime scheduling contract: request-loop drains are budgeted, foreground work yields over queued background reindexing, and explicit idle slices drain semantic diagnostics before background work.
11. Mirror lexer token additions in the editor syntax snapshot layer so highlighting, grouping, and completion contexts do not drift from compiler tokenization.
12. When async, continuation, or task syntax adds tokens such as `?|` or `||>`, update `src/StyioServices/StyioIDE/Syntax.cpp` in the same change so editor highlighting and diagnostics recognize the new token boundary, then prove accepted grammar through the compiler parser path.
13. When testing `VFS` close/drop-open-file behavior, put expected closed-file contents on disk before closing the in-memory document; closed snapshots intentionally reload from disk instead of retaining stale open-buffer query state.
14. Keep compiler bridge code pointed at the shared compiler stage entry `styio::sema::require_semantic_analysis(...)` with `AstToStyioIRLowerer` as the semantic context; do not call AST visitor hooks directly from IDE orchestration and do not rebuild a separate IDE analyzer. The historical `StyioAnalyzer` compatibility alias has been removed.
15. Treat `StyioSyntaxDrift.CorpusMatchesApprovedEnvelope` as an approved-drift ledger, not a permanent suppression list. If compiler strict parsing accepts a corpus case and the compiler outline matches the editor syntax outline, remove the approved exception; if strict parsing rejects malformed source, the exception must say the editor snapshot is non-authoritative.
16. `analyze_document` must not use recovery parsing to publish later semantic facts from malformed source. Syntax-validity consumers should use `styio check --syntax --json --file`, not IDE token/CST snapshots.
17. IDE and LSP diagnostics must preserve shared Styio diagnostic identity: compiler facts carry compiler/service codes, editor-only facts use `styio-editor` service codes, and LSP publishes `Diagnostic.code` plus `data.phase`. If an internal diagnostic carries a Styio code but no explicit phase, the LSP layer derives `data.phase` from the code prefix instead of dropping the phase.
18. Treat Vityo and Spio as first-party service consumers, not generic LSP-only clients. They may use deep convenience adapters over `StyioServices`, but those adapters must reuse shared service facts and must not become separate grammar, diagnostic, or semantic authorities.
19. Host-facing service payloads should preserve `documentId`, `revision`, `protocolVersion`, `toolchainId`, `parserEngine`, `grammarVersion`, `configPath`, `workingDirectory`, and per-capability state whenever those fields apply.
20. Keep `StyioIDECommon.*` tests covering URI/path conversion, `TextBuffer` offset mapping, range helpers, and host-facing enum string contracts when those common service helpers change.
21. IDE/LSP Windows compatibility changes must keep `styio_lspd` and `styio_ide_test` buildable with native CMake generators. Test fixtures that touch paths, environment variables, subprocesses, or executable names should use portable helpers so IDE service validation does not depend on POSIX shells or Unix path semantics on Windows.

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
