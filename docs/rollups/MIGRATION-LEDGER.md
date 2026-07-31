# Migration Ledger

**Purpose:** Track every active historical-compatibility migration so the project can reduce historical burden checkpoint by checkpoint without losing visibility on the still-open seams. Each row has an explicit completion signal so the seam can be retired the day its closure conditions are met.

**Last updated:** 2026-06-28

> Search code, scripts, and docs for `MIGRATION-NEEDED:` to find every site annotated under this ledger.

## How To Use

1. Before adding a new compatibility shim, alias, fallback engine, or "legacy" branch, register it here with its completion signal.
2. When closing a migration, remove the matching `MIGRATION-NEEDED:` markers from the code, then drop the row from this ledger.
3. Each row links to its owner runbook and to the file it lives in. Owners are responsible for keeping the row honest.
4. This ledger does not replace `NEXT-STAGE-GAP-LEDGER.md`. Migrations carry both an end-state plan (here) and any open implementation gap (there) only when the gap is broader than the migration itself.

## Open Migrations

| ID | Name | Owner | Site(s) | Completion Signal |
|----|------|-------|---------|-------------------|
| M-PARSER-01 | Parser engine `Legacy` 鈫?`Nightly` | Frontend | `src/StyioParser/Parser.hpp:29-34` (`enum StyioParserEngine { Legacy, Nightly, New = Nightly }`); `src/StyioParser/Parser.cpp` and `src/StyioParser/NewParserExpr.cpp` `_latest` function family; `src/StyioParser/Parser.cpp:5485` and `NewParserExpr.cpp:2598` legacy import diagnostic; `Parser.cpp:1689` and `NewParserExpr.cpp:1703` retired state-resource diagnostics | `Legacy` enum value, `New` alias, `_latest` suffix sweep, and `nightly_internal_legacy_bridge*` counters all removed; `NewParserExpr.cpp` either deleted or merged back into `Parser.cpp`. |
| M-PARSER-02 | Profiler legacy-bridge counters | Frontend, Codegen / Runtime | `src/StyioProfiler/FrontendProfiler.hpp:48,86` `nightly_internal_legacy_bridge*`; `src/StyioParser/Parser.hpp:42-48` `legacy_fallback_statements`, `nightly_internal_legacy_bridges` | Migrations M-PARSER-01 closed and the counters drop from the public profiler surface. |
| M-SEMA-01 | Lowering rejects retired AST forms | Sema / IR | `src/StyioLowering/AstToStyioIR.cpp:2085` (`InfiniteAST` rejection), `:2781` (`ReadFileAST` rejection); `src/StyioAST/AST.hpp:4551,4826` "Legacy" comments | Either remove the AST nodes (`InfiniteAST`, `ReadFileAST`) entirely once the syntax is fully retired, or replace the rejections with real lowering paths. |
| M-RUNTIME-01 | `StyioRuntime/` partial subsystem split | Codegen / Runtime | `src/StyioRuntime/HandleTable.hpp`, `RuntimeState.*`, and `ReadyQueue.hpp`; `src/main.cpp:5185` `// C.1 shell: handle table exists before runtime migration.` | Runtime state and task ready-queue policy now live below `StyioRuntime/`; close this migration only when the remaining collection, string, file, task/profiling, and resource-lifetime implementations move behind runtime modules or the ledger records why each stays in `StyioExtern`. |
| M-RUNTIME-02 | `Checkpoint C.1/C.2 shell` session lifetime | Codegen / Runtime | `src/StyioSession/CompilationSession.hpp:28-31`, `src/StyioSession/SessionAllocation.hpp` (header-only), `src/main.cpp:5185` | The shell comment is removed when the runtime/session migration plan in the rollup ledger reports closure. |
| M-CLI-01 | `src/main.cpp` is too large | CLI / Nano | `src/main.cpp` (5,685 LOC; embedded TOML project-config parser, styio-nano package/publish/manifest workflow, `--machine-info=json` printer, parser shadow-compare driver) | Non-CLI logic is relocated under `StyioServices/StyioConfig/` or a new `src/StyioCLI/` so `main.cpp` shrinks to ~200-500 lines. |
| M-AUDIT-01 | Stale 2026-04-22 audit reports | Docs / Ecosystem | `docs/audit/EXTERNAL-AUDIT-2026-04-22.md`, `docs/audit/agent-findings/nightly-compiler-2026-04-22.md`, `docs/audit/agent-findings/nightly-ide-parser-2026-04-22.md`, `docs/audit/agent-findings/nightly-sema-codegen-2026-04-22.md` | Apply all four sub-steps in the same commit: (1) verify each finding is closed against `docs/adr/IMPLEMENTED-DECISIONS.md` and the matching `docs/rollups/IM-D*` inventory; (2) record the closure rows in `docs/archive/ARCHIVE-MANIFEST.json`; (3) regenerate `docs/archive/ARCHIVE-LEDGER.md`; (4) `git rm` the four files listed in the Site(s) cell. |
| M-PLAN-02 | `docs/history/2026-05-19.md` snapshot | Docs / Ecosystem | `docs/history/2026-05-19.md` (single dated note explicitly referenced by `NEXT-STAGE-GAP-LEDGER.md`) | The owning ledger row closes; the snapshot is archived through the manifest and removed. |

## Recently Closed (2026-06-28 implementation-gap pass)

| Migration | Closure |
|-----------|---------|
| `GetTypeIO.cpp` placeholder type | Removed the `MIGRATION-NEEDED` marker and the `i64` stand-ins for merged legacy IO nodes: `SIOPath` now reports an LLVM pointer type, while statement-shaped `SIOPrint` and `SIORead` report `void`. Focused coverage lives in `StyioCodeGenInternal.LegacyIoGetTypeUsesConcreteStatementAndPathTypes`. |
| Pipeline-check legacy `printf/puts` canonicalization | Removed the `MIGRATION-NEEDED` marker and the old Layer 4 normalization branches for `@printf`, `@puts`, `@styio_fmt_i64`, and `@styio_fmt_str` after repository codegen had already moved stdout lowering to `styio_stdout_write_cstr`. `StyioFiveLayerPipeline.ReportsAstIrLlvmStdoutAndStderrGoldenFailures` now relies on current-helper LLVM mismatch coverage instead of feeding legacy IR text. |
| `CHANGELOG.md` archive-policy wording | Closed the stale `M-DOCS-01` row after verifying `CHANGELOG.md` no longer contains the old "Draft language experiments live under docs/archive/" wording and the current Unreleased note points to active migration-ledger provenance instead of treating `docs/archive/` as a raw-history retention area. |
| `std.resource` prelude location | Moved the data-only resource prelude from `src/StyioPrelude/resources.styio` to `share/styio/prelude/resources.styio`, updated Runtime install layout, `--source-build-info=json`, stdlib manifest evidence, nano source closure seeds, parser coverage, and owner docs so `src/Styio*/` remains C++-only. |

## Recently Closed (2026-06-19 tool and skill reduction pass)

| Migration | Closure |
|-----------|---------|
| `extend_tests.py` unreachable scaffolder | Deleted the hardcoded lit-era helper and removed active references from agent specs, team-docs mapping, and workflow mapping; feature tests now stay under `tests/features/` and CTest. |
| `scripts/perf-route.sh` and `scripts/soak-minimize.sh` wrappers | Deleted the script-level wrappers; retained only the current `benchmark/` adapters that locate `styio-benchmark` and pass `--styio-root`. |
| `scripts/ecosystem-product-gate.py` and `scripts/ecosystem-sample-workflow-gate.py` silent proxies | Deleted the silent-success proxies; fixed-revision ecosystem acceptance now runs from the Pafio-owned product matrix. |

## Recently Closed (2026-05-22 reduction pass)

| Migration | Closure |
|-----------|---------|
| `StyioAnalyzer/` compatibility shim | `src/StyioAnalyzer/ASTAnalyzer.hpp` deleted; `using StyioAnalyzer = AstToStyioIRLowerer;` and `using StyioAnalyzerVisitor = StyioSemaLoweringVisitor;` aliases removed; runbooks and `scripts/team-docs-gate.py` updated. |
| `StyioJIT/JITExecutor.hpp` empty stub | Header deleted; `docs/specs/AGENT-SPEC.md` directory tree updated. |
| `StyioIR/DFIR/` planned-but-empty subsystem | Two zero-byte files plus the directory deleted; `src/StyioToString/ToString.cpp:10` dead include removed; `src/StyioIR/IRDecl.hpp` comment refreshed. |
| Zero-byte `.cpp` placeholders | `src/StyioIR/GenIR/GenIR.cpp`, `src/StyioIR/IOIR/IOIR.cpp`, `src/StyioIR/IOIR/Planner.cpp` deleted. |
| `IOIR/IOIR.hpp` separate domain | Three classes (`SIOPath`, `SIOPrint`, `SIORead`) merged into `src/StyioIR/GenIR/SIOIR.hpp`; 5 includers updated; `src/StyioIR/IOIR/` directory deleted. |
| `src/cmake/StyioIDESources.cmake` stale alias | Pre-StyioServices reorg leftover deleted (no `include()` in repo). |
| `src/StyioCodeGen/CodeGen.cpp` 57-line shard | Merged into `CodeGenG.cpp` end-of-file; cmake source list and `src/main.cpp` source-build inventory updated. |
| Root `FindICU.cmake` location | Moved to `cmake/FindICU.cmake`; second `CMAKE_MODULE_PATH` entry removed; `cmake/StyioICU.cmake` and `docs/specs/THIRD-PARTY.md`, `AGENT-SPEC.md` references updated. |
| `tests/milestones/m11/` retired-workflow fixture | Six tracked files removed via `git rm`; the milestones workflow itself was retired on 2026-05-10. |
| `workflows/` mirror of root `workflows/` | Mirror tree deleted; root `workflows/` is the single canonical SSOT; `WORKFLOW-ORCHESTRATION.md` relocated; `scripts/workflow-scheduler.py`, `scripts/docs_config.py`, `tests/workflow_scheduler_test.py`, and ~20 docs path references rewritten; `workflows/WORKFLOW-ORCHESTRATION.toml` added. |
| `workflows/SYNTAX-ADDITION-WORKFLOW.md` superseded | Workflow superseded by `workflows/ADD-SYNTAX-WITH-SKILLS.md`; references in `docs/teams/CODEGEN-RUNTIME-RUNBOOK.md`, `docs/teams/FRONTEND-RUNBOOK.md`, `tests/workflow_scheduler_test.py`, and `docs/specs/DOCUMENTATION-POLICY.md` updated. |
| `docs/archive/adr/` empty subshell | Deleted; `docs/archive/INDEX.md` and `docs/archive/README.md` updated to reflect the simplified archive lifecycle. |
| Stray local build artifacts | `build-codex/`, `build-release-alpha/`, `cmake_test_discovery_*.json`, `scripts/__pycache__/`, `tests/.lit_test_times.txt`, `.cursor/`, `.vscode/` cleaned (~1.25 GB reclaimed; all gitignored). |

## Cross-References

1. Active gap ownership lives in [`NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md).
2. Implemented decision provenance lives in [`../adr/IMPLEMENTED-DECISIONS.md`](../adr/IMPLEMENTED-DECISIONS.md).
3. Lifecycle rules for archive state live in [`../archive/README.md`](../archive/README.md).
