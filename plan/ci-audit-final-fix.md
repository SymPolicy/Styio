# CI Gate and Audit Fix Report

**Branch:** fix/final-remote-ci
**Base:** nightly
**Date:** 2026-06-24

## Summary

This document catalogs all test failures found in the CI gate (ctest) and audit (styio-audit), identifies whether each is a new regression introduced by commit 2365ade or a pre-existing issue from the B3/B4 zero-copy tokenizer migration (commits 382bc0c, 0d8b082), and documents the fixes applied.

---

## Tests FIXED (5 root causes addressed)

### Fix 1: `StyioToken::original` field empty for zero-copy tokens

**Files changed:**
- `src/StyioTesting/PipelineCheck.cpp` -- `tokens_to_golden()`: use `tok->textString()` instead of `tok->original`
- `src/main.cpp` -- `show_tokens()`: use `tok->textString()` instead of `tok->original`
- `src/StyioToken/Token.cpp` -- `length()`: return `lexeme().size()` instead of `original.length()`
- `src/StyioToken/Token.cpp` -- `as_str()`: use `this->textString()` instead of `this->original`
- `tests/styio_test.cpp` -- `pipeline_tokens_golden_latest()`: use `tok->textString()` instead of `tok->original`

**Root cause:** The B3/B4 zero-copy token contract (commits 0d8b082, 382bc0c) made `original` field empty for all tokenizer-produced tokens. Any code reading `tok->original` got empty strings. The `StyioToken::length()` and `as_str()` methods also relied on the stale `original` field.

**Tests unblocked by this fix:**
- All 20 `StyioFiveLayerPipeline.*` tests (was: 17 failures)
- `StyioDiagnostics.DebugModePrintsSourceAndTokens`
- `StyioDiagnostics.DumpFlagsPrintAstStyioIrAndLlvmIr`
- `StyioSecurityTokenRepr.StableNamesCoverAstDataTypeOperatorAndTokenSwitches`
- `StyioSecurityParserContext.CoversLegacyContainerConditionAndLoopHelpersDirectly`
- `StyioSecurityParserContext.CoversLegacyIndexAndContainerHelpersDirectly`
- `StyioSecurityParserContext.CoversLegacyBinopTupleListAndReturnHelpersDirectly`
- `StyioSecurityParserContext.CoversLegacyAttributeCallAndRangeHelpersDirectly`
- `StyioSecurityParserContext.CoversLegacyStatementCodpIteratorAndReadFileHelpersDirectly`

### Fix 2: Nano package linker errors

**Files changed:**
- `src/main.cpp` -- `styio_nano_source_roots_latest()`: added `src/StyioSession/SymbolInterner.cpp`, `src/StyioSession/TypeTable.cpp`, `src/StyioUtil/SourceMap.cpp`

**Root cause:** Commit 2365ade wired `SymbolInterner` and `TypeTable` into `CompilationSession`, and repaired `SourceMap` UB. The nano package closure generator (`styio_nano_source_roots_latest`) did not include these new `.cpp` files, causing undefined reference linker errors in clean-room nano builds.

**Tests unblocked by this fix:**
- `StyioNanoPackage.LocalSubsetConfigMaterializesBundle`
- `StyioNanoPackage.LocalSubsetCliMaterializesBundle`

### Fix 3: Tokenizer metrics test updated

**Files changed:**
- `tests/security/styio_security_test.cpp` -- `MetricsArePopulated`: replaced `owned_text_token_count`/`owned_text_bytes` checks with `source_span_token_count`

**Root cause:** The zero-copy tokenizer never creates owned-text tokens, so `owned_text_token_count` is always zero. Updated the test to check metrics that are actually populated in the span-first tokenizer.

**Tests unblocked by this fix:**
- `StyioTokenizerContract.MetricsArePopulated`

### Fix 4: CI workflow references non-existent `styio_soak_test` target

**Files changed:**
- `.github/workflows/styio-ci-gate.yml` -- removed `styio_soak_test` from build target list

**Root cause:** The CI workflow referenced `styio_soak_test` as a build target, but no such target exists in `CMakeLists.txt` and no soak test source files exist in the repository. This would cause the CI build step to fail with `ninja: error: unknown target 'styio_soak_test'`.

### Fix 5: Docs audit updates

**Files changed:**
- `docs/teams/CLI-NANO-RUNBOOK.md` -- documented new nano source roots rule
- `docs/teams/FRONTEND-RUNBOOK.md` -- documented token `original` field deprecation
- `docs/teams/DOC-STATS.md` -- refreshed word counts
- `docs/teams/INDEX.md` -- regenerated (via `scripts/docs-index.py`)

**Root cause:** The docs audit gate failed because `src/main.cpp` and `src/StyioToken/Token.cpp` were modified without updating the corresponding team runbooks.

---

## Tests still FAILING (pre-existing, not caused by 2365ade)

These 49 test failures are pre-existing regressions from the B3/B4 zero-copy tokenizer migration (commits 382bc0c, 0d8b082) or earlier baseline issues from a490ed1.

| # | Test(s) | Count | Root Cause | Notes |
|---|---------|-------|-----------|-------|
| 1 | `StyioDiagnostics.NativeExtern*` | 8 | Nightly parser rejects `@extern` syntax; program exits at parse phase (code 3) instead of native_interop phase (code 4) | Parser limitation |
| 2 | `StyioDiagnostics.NightlyScalarContainerAndExportRoutesStayExecutable` | 1 | Parser rejects `@export { ... }` syntax | Same issue as #1 |
| 3 | `StyioDiagnostics.ImportDeclarationFailsClosedAsUnsupportedRuntimeValue` | 1 | Parser rejects `@import` syntax | Same issue as #1 |
| 4 | `StyioSecurityNightlyParserStmt.*` | 7 | Nightly parser rejects `@import`, `@export`, `@extern`, and `#` hash-statement syntax | Parser limitation |
| 5 | `StyioSecurityNightlySemantics.*` | 2 | Nightly parser rejects `# hash_stmt` syntax | Same issue as #4 |
| 6 | `StyioSecurityParserContext.CoversBoundExternManualTokenBoundariesDirectly` | 1 | Legacy parser context helper returns unexpected values | Pre-existing legacy code issue |
| 7 | `StyioParserInternal.*` | 7 | Parser internal helpers fail with null pointers or unexpected values | Pre-existing legacy parser issue |
| 8 | `StyioMainContract.LowLevelMainHelpersCoverFailureBoundaries` | 1 | CLI contract test fails | Pre-existing |
| 9 | `StyioIRContract.MainBlockResourceMethodInliningCoversValueScopesAndProperties` | 1 | IR contract test fails | Pre-existing |
| 10 | `StyioHirBuilder.CanonicalizesAtImportPaths` | 1 | IDE test fails | Pre-existing |
| 11 | `StyioNameResolver.ResolvesImportsAcrossFiles` | 1 | IDE test fails | Pre-existing |
| 12 | `StyioSemanticDb.MemberCompletionInfersReceiverTypeFromImportedFunctionSignature` | 1 | IDE test fails | Pre-existing |
| 13 | `StyioCompletionEngine.RanksLocalsAboveImportsAndBuiltins` | 1 | IDE test fails | Pre-existing |
| 14 | `scalar_expressions_t20_combined` | 1 | Arithmetic evaluates to 0 instead of correct result | Constant-fold or IR-gen regression |
| 15 | `native_interop_t*` | 8 | Nightly parser rejects `@extern` syntax | Same as #1 |
| 16 | `task_resources_t02_task_flow_right` | 1 | Arithmetic evaluates to 0 inside task body | Same as #14 |
| 17 | `stdio_output_t05_stdout_var` | 1 | Arithmetic evaluates to 0 | Same as #14 |
| 18 | `styio_build_native_executable_native_interop` | 1 | Nightly parser rejects `@extern` syntax | Same as #1 |
| 19 | `styio_core_benchmark_internal` (SEGFAULT) | 1 | Benchmark crashes on startup | Pre-existing |
| 20 | `styio_core_benchmark_json_output` (SEGFAULT) | 1 | Benchmark crashes on startup | Pre-existing |

**Total: 49 tests still failing**

---

## Audit Findings (styio-audit)

The styio-audit gate reports 3 findings, all pre-existing:

| Resource Class | Finding | Status |
|---------------|---------|--------|
| `compiler_ast_ir_ownership` | scope glob `src/StyioAnalyzer/**` matches no source file | Directory does not exist; likely planned but never created |
| `ide_lsp_workspace_state` | scope glob `src/StyioIDE/**` matches no source file | IDE code moved to `src/StyioServices/StyioIDE/` |
| `ide_lsp_workspace_state` | scope glob `src/StyioLSP/**` matches no source file | LSP code moved to `src/StyioServices/StyioLSP/` |

None of these findings are caused by the code changes in commit 2365ade or the fixes in this branch. They reflect the external audit policy not matching the current source layout. The CI workflow normalizes the IDE/LSP paths before running the audit; the Analyzer path remains unresolved externally.
