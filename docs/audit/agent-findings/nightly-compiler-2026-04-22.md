# Nightly Compiler Findings 2026-04-22

**Purpose:** Record compiler audit findings and remediation from the parallel external audit pass.

**Last updated:** 2026-04-22

**Scope:** `src/StyioAST`, `src/StyioAnalyzer`, `src/StyioCodeGen`, and compiler-facing unit coverage in `tests/`

**Status:** Partially remediated during this pass

> **MIGRATION-NEEDED: M-AUDIT-01** (docs/rollups/MIGRATION-LEDGER.md). This dated audit shard is older than the audit-retention window per `docs/audit/README.md`. Verify each finding is closed against `docs/adr/IMPLEMENTED-DECISIONS.md` / `docs/rollups/IM-D*` inventories, archive through `docs/archive/ARCHIVE-MANIFEST.json`, and remove this file from the current tree.

## Closed During This Pass

- `SizeOfAST` was a real silent-fallback path. It now owns a writable type slot, type-infers to `i64`, and lowers to `SGListLen` / `SGDictLen` instead of `SGConstInt(0)`.
- Coverage was added in `tests/security/styio_security_test.cpp` as `StyioSecurityAstOwnership.SizeOfLowersListLength`.
- Verification run:
  - `cmake --build <workspace-root>/build-codex --target styio_test styio_security_test -j8`
  - `ctest --test-dir <workspace-root>/build-codex -R 'StyioSecurityAstOwnership\\.(SizeOfOwnsValueExpr|SizeOfLowersListLength)$' --output-on-failure`
  - `ctest --test-dir <workspace-root>/build-codex -R 'StyioFiveLayerPipeline\\.P01_print_add$' --output-on-failure`

## Open Findings

| ID | Area | Severity | Evidence | Why it matters |
|----|------|----------|----------|----------------|
| SNY-NIGHTLY-CC-001 | IR lowering silent fallback | High | `src/StyioAnalyzer/ToStyioIR.cpp:1023-1793` | A large set of AST handlers still lower to `SGConstInt::Create(0)` instead of rejecting unsupported input. That makes unsupported syntax look successful and can silently poison later codegen. |
| SNY-NIGHTLY-CC-002 | Codegen fail-open defaults | High | `src/StyioCodeGen/CodeGenG.cpp:388-436`, `src/StyioCodeGen/CodeGenG.cpp:1345-1543`, `src/StyioCodeGen/CodeGenG.cpp:3556-3599` | Several unsupported IR nodes still return zero/null placeholders in codegen, and verifier failure in `execute()` only prints before returning. That is not a hard stop. |
| SNY-NIGHTLY-CC-003 | Parser / coverage gap | Medium | `tests/` currently has only a direct AST regression for `SizeOf`; there is no CLI end-to-end regression for `|expr|` on the current parser path. | The compiler path still needs a parser-level regression if `SizeOf` is meant to be user-facing from the CLI. Right now the safety net is sema/IR-only. |

## Residual Risk

The main unresolved compiler risk is still silent fallback, not arithmetic correctness. The newly fixed `SizeOf` path proves the pattern, but the broader placeholder set remains in place and is not yet gated by a dedicated negative test suite.

The current regression coverage is better than before, but it is still incomplete for unsupported-lowering and fail-open codegen branches. The direct unit coverage added here is useful, but it does not replace a parser-level or gate-level rejection matrix.
