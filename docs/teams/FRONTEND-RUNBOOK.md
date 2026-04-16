# Frontend Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of Styio tokenization, parsing, Unicode handling, and legacy/nightly parser migration; this file links to language and test SSOTs instead of redefining grammar.

**Last updated:** 2026-04-16

## Mission

Own the source-to-AST front end: token definitions, lexer behavior, parser routing, parser recovery, lookahead helpers, and legacy/nightly migration boundaries. Do not own language meaning beyond implementing the design SSOT.

## Owned Surface

Primary paths:

1. `src/StyioToken/`
2. `src/StyioUnicode/`
3. `src/StyioParser/`
4. `src/Deprecated/` only as migration reference
5. Parser-facing tests under `tests/milestones/`, `tests/fuzz/`, and parser shadow gates

Build and test targets:

1. `styio_frontend_core`
2. `styio_core`
3. `styio_test`
4. `styio_fuzz_lexer` and `styio_fuzz_parser` when fuzz is enabled

## Daily Workflow

1. Read [../design/Styio-EBNF.md](../design/Styio-EBNF.md), [../design/Styio-Symbol-Reference.md](../design/Styio-Symbol-Reference.md), and relevant language sections before changing syntax.
2. Check [../rollups/CURRENT-STATE.md](../rollups/CURRENT-STATE.md), [../rollups/NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md), and the parser gate sections in [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) when touching legacy/nightly paths; use [../archive/rollups/HISTORICAL-LESSONS.md](../archive/rollups/HISTORICAL-LESSONS.md) only if active docs are still insufficient.
3. Make lexer and parser changes in the smallest parse subset possible.
4. Add or update a failing fixture before changing accepted behavior.
5. Update [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) when adding milestone or parser acceptance coverage.

## Change Classes

1. Small: typo-safe parser helper changes, local lookahead fix, or token display-name cleanup. Run targeted unit or milestone tests.
2. Medium: new token, changed AST construction, changed parser fallback, or changed parse diagnostics. Add tests and run parser shadow gates for affected milestones.
3. High: default parser route, legacy fallback policy, statement boundary, or recovery-mode behavior. Use checkpoint workflow, add ADR if ownership or route policy changes, and run checkpoint health.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build -L milestone
ctest --test-dir build -R '^StyioParserEngine\.'
ctest --test-dir build -R '^parser_shadow_gate_'
```

When touching fuzz-sensitive boundaries:

```bash
cmake -S . -B build-fuzz -DSTYIO_ENABLE_FUZZ=ON
cmake --build build-fuzz --target styio_fuzz_lexer styio_fuzz_parser
ctest --test-dir build-fuzz -L fuzz_smoke
```

For checkpoint-grade validation:

```bash
./scripts/checkpoint-health.sh --no-asan
```

## Cross-Team Dependencies

1. Sema / IR must review changes that alter AST shape, node ownership, or parse-mode recovery output.
2. Test Quality must review new parser acceptance fixtures, shadow gate changes, and fuzz regression samples.
3. IDE / LSP and Grammar must review changes that affect edit-time syntax assumptions or public diagnostics.
4. Docs / Ecosystem must review changes to language SSOT links or parser migration workflow docs.

## Handoff / Recovery

Record unfinished parser work in `docs/history/YYYY-MM-DD.md` with:

1. Parser engine, route, and feature subset.
2. Exact failing command or shadow artifact path.
3. Legacy/nightly fallback status.
4. Next smallest parser slice.
5. Rollback point if accepted syntax changed.
