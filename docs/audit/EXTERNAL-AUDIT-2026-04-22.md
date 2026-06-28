# External Audit 2026-04-22

**Purpose:** External `styio-audit` review of `styio-nightly` under project `styio`, loaded with `for-styio`.

**Last updated:** 2026-06-28

> **MIGRATION-NEEDED: M-AUDIT-01** (docs/rollups/MIGRATION-LEDGER.md). This dated audit report is older than the audit-retention window per `docs/audit/README.md`. Findings have been absorbed into `docs/adr/IMPLEMENTED-DECISIONS.md` and the `docs/rollups/IM-D*` inventories; verify each finding closure, archive through `docs/archive/ARCHIVE-MANIFEST.json`, and remove this file from the current tree.


## Scope

This audit checked the seven design principles in `docs/specs/audit/CODE-AUDIT-CHECKLIST.md`, test coverage gaps, resource and state-machine lifecycle handling, delivery-gate strictness, and the security, performance, and correctness risks called out by the current code.

## Verification

- `python3 <styio-audit-bin> --framework-root <styio-audit-root> --format text gate --repo <workspace-root> --project styio` failed because `docs/audit/defects/STYIO-NIGHTLY-2026-04-22.md` is still `Open`.
- `cmake --build <workspace-root>/build-codex --target styio_test styio_ide_test styio_security_test -j8` completed with no rebuild needed.
- `ctest --test-dir <workspace-root>/build-codex -R 'StyioTypes\\.GetMaxTypeNumericPromotionByBitWidth|StyioLspServer\\.RunDrainsRuntimeDiagnostics|StyioLspRuntime\\.RuntimeDrainCanBeBudgetedForScheduling' --output-on-failure` passed.
- `./scripts/checkpoint-health.sh --build-dir build-codex --no-asan --no-fuzz` passed after syncing `docs/audit/INDEX.md` with the new report entry.

## Positive Coverage

- `tests/styio_test.cpp` now covers numeric promotion behavior in `getMaxType`, which reduces the chance of reintroducing the current type-widening bug in `src/StyioToken/Token.cpp`.
- `tests/ide/styio_ide_test.cpp` now covers runtime drain budgeting in the IDE/LSP loop and confirms the new `Server::drain_runtime(...)` path.
- `scripts/checkpoint-health.sh` now builds `styio_ide_test` and exercises the new IDE/LSP runtime scheduling tests, so the gate no longer ignores those changes.

## Parallel Remediation Shards

- [Nightly Compiler Findings 2026-04-22](./agent-findings/nightly-compiler-2026-04-22.md) closed the `SizeOfAST` silent fallback by giving `SizeOf` a writable type slot and lowering it to `SGListLen` / `SGDictLen`, with direct security coverage.
- [Nightly IDE / Parser Audit Shard](./agent-findings/nightly-ide-parser-2026-04-22.md) closed stale closed-file snapshots, persistent index resurrection, and malformed/oversized LSP frame handling.
- [Nightly Sema / Codegen Fail-Closed Findings 2026-04-22](./agent-findings/nightly-sema-codegen-2026-04-22.md) closed unknown-call, arity-mismatch, `SGCall`, runtime list-operation, and LLVM verifier fail-open paths with focused sema/codegen regressions.

## Findings

| ID | Principles | Severity | Evidence | Why it matters | Coverage / gate status |
|----|------------|----------|----------|----------------|------------------------|
| SNY-AUD-001 | 5, 7 | Closed | `src/main.cpp`, `tests/main_contract_test.cpp` | Nano subprocesses now resolve tool names to concrete executable paths before argv/exec launch, tar blobs are listing-validated before extraction, and extracted or published package trees reject traversal escapes and symlinks before archive copy/publish. | Covered by `StyioMainContract.NanoArchiveBoundariesRejectTraversalAndSymlinks` plus the process-tool resolution check in `StyioMainContract.NativeCompilerFallbackAndHttpFetchHelpersStayExplicit`. |
| SNY-AUD-003 | 5, 4 | Closed | `src/main.cpp`, `tests/main_contract_test.cpp` | Nano source-closure include containment now uses path-component relative checks instead of string-prefix matching, so sibling roots such as `source_sibling` cannot be captured through `..` include targets. | Covered by `StyioMainContract.NanoSourceClosureHelpersCoverSuccessAndFailureEdges`. |
| SNY-AUD-004 | 1, 2, 5, 7 | Closed | `src/StyioServices/StyioIDE/Project.cpp`, `tests/ide/styio_ide_test.cpp` | Project cache roots now use stable `root-<hex-path>` identities and workspace scans use `std::filesystem` error-code traversal instead of throwing or hiding missing-root failures. | Covered by `StyioIdeProject.EnvironmentFallbacksAndWorkspaceSkipsStayExplicit`; IM-D9 records the cache-root contract. |
| SNY-AUD-005 | 4, 6 | Closed | `src/StyioLowering/AstToStyioIR.cpp`, `src/StyioCodeGen/CodeGenG.cpp`, `tests/security/styio_security_test.cpp`, `tests/codegen_internal_test.cpp` | Direct unsupported AST lowering fails closed through `unsupported_ast_lowering(...)`, and collection/list/dict/matrix codegen value coercion now rejects mismatched LLVM value families instead of synthesizing zero, null, or fake handles. | Covered by `StyioIRContract.UnsupportedAstNodesFailClosedInsteadOfPlaceholder` and `StyioCodeGenInternal.CollectionValueMismatchesFailClosed`. |
| SNY-AUD-006 | 4 | Closed for call path | `src/StyioAnalyzer/TypeInfer.cpp`, `src/StyioAnalyzer/ToStyioIR.cpp`, `src/StyioCodeGen/CodeGenG.cpp` | Unknown user calls now fail closed during typecheck/lowering and defensively in codegen. | Covered by `StyioSecurityNightlySemantics.RejectsUnknownFunctionDuringTypecheck` and `StyioSecurityNightlyCodegen.UnknownSgCallFailsClosed`. |
| SNY-AUD-007 | 4 | Closed for call path | `src/StyioAnalyzer/TypeInfer.cpp`, `src/StyioAnalyzer/ToStyioIR.cpp`, `src/StyioCodeGen/CodeGenG.cpp` | User-call and runtime list-operation arity mismatches now throw before LLVM emission instead of falling back to integer `0`. | Covered by exact-arity sema/codegen regressions in `tests/security/styio_security_test.cpp`. |
| SNY-AUD-008 | 4, 6 | Closed for `execute()` | `src/StyioCodeGen/CodeGen.cpp` | LLVM verifier failure in `StyioToLLVM::execute()` now throws instead of printing and returning. | Covered by the sema/codegen shard; other verifier entry points should still be audited before declaring the broader class closed. |
| SNY-AUD-009 | 4, 5 | Closed | `src/StyioCodeGen/CodeGenG.cpp`, `tests/codegen_internal_test.cpp` | Integer `/`, `%`, `/=`, and `%=` now replace unsafe operands before LLVM `sdiv` / `srem` emission, so zero or undefined divisors cannot reach UB-producing instructions. | Covered by `StyioCodeGenInternal.IntegerDivAndModuloGuardUnsafeDivisorsBeforeInstruction`; codegen runbook records the IR-shape rule. |
| SNY-AUD-011 | 3, 6 | Closed | `src/StyioServices/StyioIDE/SemDB.cpp`, `src/StyioServices/StyioIDE/Common.cpp`, `tests/ide/styio_ide_test.cpp` | Semantic token positions and lengths now use `TextBuffer` UTF-16 helpers before LSP delta encoding, while internal IDE offsets remain byte-based. | Covered by `StyioSemanticDb.SemanticTokensUseUtf16PositionsAndLengths` and the `TextBuffer` emoji/CJK position helper regression. |
| SNY-AUD-012 | 3, 5 | Closed | `src/StyioLSP/Server.cpp:247-322` | `Content-Length` parsing now validates, caps oversized frames, drains safely, and ignores overflowing string request IDs without throwing. | Covered by `StyioLspServer.SkipsMalformedFramesAndHandlesLargeStringIds`. |
| SNY-AUD-013 | 1, 5, 7 | Closed | `src/StyioServices/StyioLSP/Server.cpp`, `src/StyioServices/StyioIDE/Service.cpp`, `tests/ide/styio_ide_test.cpp` | `workspace/didChangeWatchedFiles` now extracts concrete changed URIs, filters to closed `.styio` files under the selected workspace root, and coalesces duplicates through the background-index queue. Empty, non-Styio, open-file, and out-of-workspace events are no-ops. | Covered by `StyioIdeService.WatchFileRefreshFiltersAndCoalescesChangedPaths` and `StyioLspServer.WatchFileChangesFilterAndCoalesceBackgroundRefresh`. |

## Gate Assessment

The gate is stricter than before because `scripts/checkpoint-health.sh` now exercises the IDE/LSP runtime scheduling tests, and focused security/IDE tests now cover several previously fail-open compiler and LSP paths.

The focused gates now cover the previously open audit surface in this dated report: unsupported AST lowering, collection/codegen placeholder coercion, CLI/package archive boundaries, IDE cache/scan behavior, LSP watched-file filtering, UTF-16 semantic tokens, and integer division/modulo UB guards.

## Residual Risk

The external `styio-audit` full gate remains blocked by the open defect record in `docs/audit/defects/STYIO-NIGHTLY-2026-04-22.md`. This report no longer tracks an open code-risk item; broader language-design and migration debt continues to live in the rollup ledgers.
