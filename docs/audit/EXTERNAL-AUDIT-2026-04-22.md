# External Audit 2026-04-22

**Purpose:** External `styio-audit` review of `styio-nightly` under project `styio`, loaded with `for-styio`.

**Last updated:** 2026-04-22

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
| SNY-AUD-001 | 5, 7 | High | `src/main.cpp:1202`, `src/main.cpp:1605`, `src/main.cpp:1623` | Artifact fetch, shell command execution, and archive handling still depend on shell parsing and process lookup. This is a security boundary, not a convenience path. | No archive-containment gate or argv-based execution gate exists for these flows. |
| SNY-AUD-003 | 5, 4 | Medium | `src/main.cpp:1975-1982` | Include containment still uses a string prefix check, so sibling paths can slip through when roots share a prefix. | Needs a sibling-prefix regression test and a component-aware path check. |
| SNY-AUD-004 | 1, 2, 5, 7 | Medium | `src/StyioIDE/Project.cpp:10-24`, `src/StyioIDE/Project.cpp:28-48` | Workspace scanning and cache identity are still implicit. `std::hash` is not a durable cache key, and recursive iteration has no explicit error state. | No explicit workspace state machine or iterator-error regression exists. |
| SNY-AUD-005 | 4, 6 | High | `src/StyioAnalyzer/ToStyioIR.cpp:1023-1793`, `src/StyioCodeGen/CodeGenG.cpp:3556-3599` | Several non-call unsupported AST/codegen paths can still synthesize placeholder zero/null values. | `SizeOfAST` and call-related paths are now covered; the remaining gap is a broader unsupported-lowering negative matrix. |
| SNY-AUD-006 | 4 | Closed for call path | `src/StyioAnalyzer/TypeInfer.cpp`, `src/StyioAnalyzer/ToStyioIR.cpp`, `src/StyioCodeGen/CodeGenG.cpp` | Unknown user calls now fail closed during typecheck/lowering and defensively in codegen. | Covered by `StyioSecurityNightlySemantics.RejectsUnknownFunctionDuringTypecheck` and `StyioSecurityNightlyCodegen.UnknownSgCallFailsClosed`. |
| SNY-AUD-007 | 4 | Closed for call path | `src/StyioAnalyzer/TypeInfer.cpp`, `src/StyioAnalyzer/ToStyioIR.cpp`, `src/StyioCodeGen/CodeGenG.cpp` | User-call and runtime list-operation arity mismatches now throw before LLVM emission instead of falling back to integer `0`. | Covered by exact-arity sema/codegen regressions in `tests/security/styio_security_test.cpp`. |
| SNY-AUD-008 | 4, 6 | Closed for `execute()` | `src/StyioCodeGen/CodeGen.cpp` | LLVM verifier failure in `StyioToLLVM::execute()` now throws instead of printing and returning. | Covered by the sema/codegen shard; other verifier entry points should still be audited before declaring the broader class closed. |
| SNY-AUD-009 | 4, 5 | High | `src/StyioCodeGen/CodeGenG.cpp:657` | Division and modulo still lack a zero-divisor guard. | Needs literal-zero and runtime-zero regressions. |
| SNY-AUD-011 | 3, 6 | High | `src/StyioIDE/SemDB.cpp:1034-1051`, `src/StyioIDE/Common.cpp:90` | Semantic token positions are still byte-based instead of UTF-16 units. This breaks LSP correctness for non-ASCII text. | No emoji/CJK semantic-token regression is in the main gate. |
| SNY-AUD-012 | 3, 5 | Closed | `src/StyioLSP/Server.cpp:247-322` | `Content-Length` parsing now validates, caps oversized frames, drains safely, and ignores overflowing string request IDs without throwing. | Covered by `StyioLspServer.SkipsMalformedFramesAndHandlesLargeStringIds`. |
| SNY-AUD-013 | 1, 5, 7 | Medium | `src/StyioLSP/Server.cpp:364-366`, `src/StyioIDE/Service.cpp:265-270` | `workspace/didChangeWatchedFiles` still schedules background refresh on every notification. The new budgeted drain helps, but path filtering and coalescing are still missing. | The new checkpoint-health addition covers runtime scheduling, not watch-event filtering or flood control. |

## Gate Assessment

The gate is stricter than before because `scripts/checkpoint-health.sh` now exercises the IDE/LSP runtime scheduling tests, and focused security/IDE tests now cover several previously fail-open compiler and LSP paths.

The gate is still not strict enough for the remaining audit surface because it does not yet cover:

1. UTF-16 semantic-token correctness
2. workspace watch-event filtering and flood control
3. non-call unsupported-syntax failure paths that currently lower to placeholder values
4. division/modulo zero-divisor rejection

## Residual Risk

The external `styio-audit` full gate remains blocked by the open defect record in `docs/audit/defects/STYIO-NIGHTLY-2026-04-22.md`. The remaining tracked code risk is now concentrated in CLI/package shell boundaries, non-call parser/codegen placeholder fallback, zero-divisor handling, UTF-16 LSP correctness, and watch-event flood control.
