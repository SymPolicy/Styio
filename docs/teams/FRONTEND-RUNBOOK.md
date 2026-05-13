# Frontend Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of Styio tokenization, parsing, Unicode handling, and legacy/nightly parser migration; this file links to language and test SSOTs instead of redefining grammar.

**Last updated:** 2026-05-13

## Mission

Own the source-to-AST front end: token definitions, lexer behavior, parser routing, parser recovery, lookahead helpers, and legacy/nightly migration boundaries. Do not own language meaning beyond implementing the design SSOT.

## Owned Surface

Primary paths:

1. `src/StyioToken/`
2. `src/StyioUnicode/`
3. `src/StyioParser/`
4. Git history only when a deleted parser snapshot is needed for migration reference
5. Parser-facing tests under `tests/features/`, `tests/fuzz/`, and parser shadow gates

Build and test targets:

1. `styio_frontend_core`
2. `styio_core`
3. `styio_test`
4. `styio_fuzz_lexer` and `styio_fuzz_parser` when fuzz is enabled

## Daily Workflow

1. Read [../design/Styio-EBNF.md](../design/Styio-EBNF.md), [../design/Styio-Symbol-Reference.md](../design/Styio-Symbol-Reference.md), and relevant language sections before changing syntax.
2. Check [../rollups/CURRENT-STATE.md](../rollups/CURRENT-STATE.md), [../rollups/NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md), and the parser gate sections in [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) when touching legacy/nightly paths; use Git history only if active docs are still insufficient.
3. Make lexer and parser changes in the smallest parse subset possible.
4. Add or update a failing fixture before changing accepted behavior.
5. Update [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) when adding feature or parser acceptance coverage.
6. When token or primitive spelling tables change, add a focused regression so public token names do not drift silently.
7. When accepted syntax reaches lowering or runtime helpers, follow [../assets/workflow/SYNTAX-ADDITION-WORKFLOW.md](../assets/workflow/SYNTAX-ADDITION-WORKFLOW.md) and do not stop at parser-only green status.
8. Conditional infinite loops use `[...] >> ?(cond) => { ... }`; reject the older `[...] ?(cond) >> { ... }` spelling in both legacy and nightly parser routes.
9. Keep negative numeric literals as literal atoms in both parser routes; `-1 + 2` must parse as `(-1) + 2`, not `0 - (1 + 2)`.
10. When a type annotation admits a collection-shaped literal, keep the parser change context-triggered, such as `m: matrix = [[...], [...]]`, and leave untyped nested list literals on the ordinary list path.
11. Match syntax surfaces such as `#(name = expr) ?=`, all-underscore default wildcards, and guarded integer arms need route-gate coverage in both parser routes before lowering asserts semantic equivalence.
12. Internal resource declarations use `@ name [: type] := #(args) => { ... }`; parser changes must enforce explicit parameters before body-name use and reject hidden pseudo-primitives such as `file(path)`.
13. Task launch and await syntax is symbol-only: `||> { ... }` constructs one task block, `||> [ t1 := { ... } t2 := { ... } ]` launches a task group, and `?| job -> answer: T | fallback` awaits a task/future handle into a newly declared local. Bare `?| -> answer: T` is a reserved continuation freeze shape and must remain fail-closed until continuation lowering exists.
14. Function-level match sugar `# name := (single_param: T) ?= { ... }` is parser-local normalization only: require exactly one parameter and construct the same `MatchCasesAST(NameAST(param), cases)` body that explicit match syntax would feed to lowering.
15. Retired state-resource state families are parser errors. Keep `@name : Type`, `expr -> @name`, `@name[-1]`, and standalone `(<< @file(...))` expression routes green in both parser routes where shadow compatibility still applies, and do not let postfix parsing cross a line break into the next statement.
16. Resource method syntax is parser-owned but sema-resolved: accept `@file::name = ...`, `@file::name := ...`, compatible `@file.name = ...` definitions, `@("path")` file-resource expressions, `@()` empty-resource sinks, and expression postfix calls such as `@("path").close()`. Inside a resource method body, bare `@file` and postfix forms such as `@file.dispose()` are the receiver instance for that method family, not a constructor.
17. Typed stdin pull syntax is one parser route: `name[, name...] <- @stdin : T-or-(T, ...)`. Scalar pulls and single-target collection pulls such as `xs <- @stdin : list[i64]` must both lower through typed `InstantPull`.
18. `$` remains syntax-sensitive: `$"..."` is the accepted format-string route and must stay green in both parser engines, while `$identifier` is retired state-resource state syntax and must keep the migration diagnostic.
19. Tokenizer and parser recovery paths are sanitizer-sensitive. Accumulate tokens, top-level statements, hash-function parts, parsed return-type fragments, parenthesized expressions, call arguments, and match-case arms/default bodies behind RAII ownership before releasing them to the session or final AST node, and backflow minimized fuzz samples into `tests/fuzz/corpus/` when nightly fuzz exposes a lifetime bug.
20. Typed annotation recovery is part of the same ownership contract. Keep parsed `TypeAST`, declared `VarAST`, await targets, resource declaration slots, and parameter nodes behind local owners until the parser has seen the required delimiter or assignment token and the final AST node has adopted them.
21. Parser recovery resource limits are fail-closed. Deep delimiter nesting, repeated fallback routes, and unclosed expression contexts must raise the parser resource-limit diagnostic directly instead of being swallowed by legacy fallback; minimized OOM fuzz seeds belong in `tests/fuzz/corpus/` with a deterministic security regression.
22. List-element recovery must stay on the `ParseAttempt` bridge. When nightly expression parsing inside list literals needs legacy fallback, route it through `try_parse_expr_subset_until_latest(...)` with explicit follow-token delimiters instead of calling the nightly subset parser directly and rewinding into `parse_expr(context)`, or deep malformed nests can bounce between routes until timeout instead of tripping the resource-limit diagnostic.
23. Internal nightly-to-legacy bridges are recovery-budgeted per token. If one cursor position declines or falls back repeatedly, raise a parser resource-limit diagnostic instead of letting list/dict/hash recovery loop between nightly subset entry and legacy expression parsing.

## Change Classes

1. Small: typo-safe parser helper changes, local lookahead fix, or token display-name cleanup. Run targeted unit or feature tests.
2. Medium: new token, changed AST construction, changed parser fallback, or changed parse diagnostics. Add tests and run parser shadow gates for affected feature labels.
3. High: default parser route, legacy fallback policy, statement boundary, or recovery-mode behavior. Use checkpoint workflow, add ADR if ownership or route policy changes, and run checkpoint health.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build/default -L language_feature
ctest --test-dir build/default -R '^StyioParserEngine\.'
ctest --test-dir build/default -R '^parser_shadow_gate_'
```

When touching fuzz-sensitive boundaries:

```bash
cmake -S . -B build/fuzz -DSTYIO_ENABLE_FUZZ=ON
cmake --build build/fuzz --target styio_fuzz_lexer styio_fuzz_parser
ctest --test-dir build/fuzz -L fuzz_smoke
```

For checkpoint-grade validation:

```bash
./scripts/checkpoint-health.sh --no-asan
```

When syntax delivery adds or renames runtime helper calls:

```bash
python3 scripts/runtime-surface-gate.py
```

## Cross-Team Dependencies

1. Sema / IR must review changes that alter AST shape, node ownership, or parse-mode recovery output.
2. Test Quality must review new parser acceptance fixtures, shadow gate changes, and fuzz regression samples.
3. IDE / LSP and Grammar must review changes that affect edit-time syntax assumptions or public diagnostics.
4. Codegen / Runtime must review syntax changes that add, rename, or reroute `styio_*` runtime helpers.
5. Docs / Ecosystem must review changes to language SSOT links or parser migration workflow docs.

## Handoff / Recovery

Record unfinished parser work in `docs/history/YYYY-MM-DD.md` with:

1. Parser engine, route, and feature subset.
2. Exact failing command or shadow artifact path.
3. Legacy/nightly fallback status.
4. Next smallest parser slice.
5. Rollback point if accepted syntax changed.
