# Frontend Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of Styio tokenization, parsing, Unicode handling, and the authoritative nightly parser contract; this file links to language and test SSOTs instead of redefining grammar.

**Last updated:** 2026-05-24

## Mission

Own the source-to-AST front end: token definitions, lexer behavior, parser routing, parser diagnostics, lookahead helpers, and the authoritative nightly parser boundary. Do not own language meaning beyond implementing the design SSOT.

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
2. Check [../rollups/CURRENT-STATE.md](../rollups/CURRENT-STATE.md), [../rollups/NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md), [../rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md](../rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md), and the parser gate sections in [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md) when touching parser authority paths; use Git history only if active docs are still insufficient.
3. Make lexer and parser changes in the smallest parse subset possible.
4. Add or update a failing fixture before changing accepted behavior.
5. Update [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md) when adding feature or parser acceptance coverage.
6. When token or primitive spelling tables change, add a focused regression so public token names do not drift silently.
7. When accepted syntax reaches lowering or runtime helpers, follow [../../workflows/ADD-SYNTAX-WITH-SKILLS.md](../../workflows/ADD-SYNTAX-WITH-SKILLS.md) and do not stop at parser-only green status.
8. Conditional infinite loops use `[...] >> ?(cond) => { ... }`; reject the older `[...] ?(cond) >> { ... }` spelling in both legacy and nightly parser routes.
9. Keep negative numeric literals as literal atoms in both parser routes; `-1 + 2` must parse as `(-1) + 2`, not `0 - (1 + 2)`.
10. When a type annotation admits a collection-shaped literal, keep the parser change context-triggered, such as `m: matrix = [[...], [...]]`, and leave untyped nested list literals on the ordinary list path.
11. Match syntax surfaces such as `#(name = expr) ?=`, all-underscore default wildcards, and guarded integer arms need route-gate coverage in both parser routes before lowering asserts semantic equivalence.
12. Internal resource declarations use `@ name [: type] := #(args) => { ... }`; parser changes must enforce explicit parameters before body-name use and reject hidden pseudo-primitives such as `file(path)`.
13. Task launch and await syntax is symbol-only: `||> { ... }` constructs one task block, `||> [ t1 := { ... } t2 := { ... } ]` launches a task group, and `?| job -> answer: T | fallback` awaits a task/future handle into a newly declared local. `?| resource_operation | fallback` is also the only resource fallback surface; `?| resource_operation | effect => handler` handles only that typed effect family, such as `backpressure`, and handler chains are allowed. Bare `| fallback` is not resource fallback. `?| op | ...` is accepted only as a standalone discard statement that produces no value; parsers must reject it in assignment, call-argument, branch-value, and other expression contexts. `?| op | effect => @()` must still be rejected because `@()` is not an executable empty action. Without fallback, `?| job -> answer: T` or `?| resource_operation` settles in place and raises immediately on failure. Bare `?| -> answer: T` is a reserved continuation freeze shape and must remain fail-closed until continuation lowering exists.
14. Function-level match sugar `# name := (single_param: T) ?= { ... }` is parser-local normalization only: require exactly one parameter and construct the same `MatchCasesAST(NameAST(param), cases)` body that explicit match syntax would feed to lowering.
15. Retired state-resource state families are parser errors. Keep `@name : Type`, `expr -> @name`, `@name[-1]`, and standalone `(<< @file(...))` expression routes green in both parser routes where shadow compatibility still applies, and do not let postfix parsing cross a line break into the next statement.
16. Resource method syntax is parser-owned but sema-resolved: accept `@file::name = ...`, `@file::name := ...`, compatible `@file.name = ...` definitions, `@("path")` file-resource expressions, `@()` empty-resource sinks, and expression postfix calls such as `@("path").close()`. Inside a resource method body, bare `@file` and postfix forms such as `@file.dispose()` are the receiver instance for that method family, not a constructor.
17. Typed stdin pull syntax is one parser route: `name[, name...] <- @stdin : T-or-(T, ...)`. Scalar pulls and single-target collection pulls such as `xs <- @stdin : list[i64]` must both lower through typed `InstantPull`.
18. `$` remains syntax-sensitive: `$"..."` is the accepted format-string route and must stay green in both parser engines, while `$identifier` is retired state-resource state syntax and must keep the migration diagnostic.
19. Tokenizer and parser recovery paths are sanitizer-sensitive. Accumulate tokens, top-level statements, hash-function parts, parsed return-type fragments, parenthesized expressions, call arguments, and match-case arms/default bodies behind RAII ownership before releasing them to the session or final AST node, and backflow minimized fuzz samples into `tests/fuzz/corpus/` when nightly fuzz exposes a lifetime bug.
20. Typed annotation recovery is part of the same ownership contract. Keep parsed `TypeAST`, declared `VarAST`, await targets, resource declaration slots, and parameter nodes behind local owners until the parser has seen the required delimiter or assignment token and the final AST node has adopted them.
21. Parser resource limits are fail-closed. Deep delimiter nesting and unclosed expression contexts must raise parser diagnostics directly instead of being swallowed by legacy fallback; minimized OOM fuzz seeds belong in `tests/fuzz/corpus/` with a deterministic security regression.
22. Accepted grammar is no-fallback. When nightly expression, statement, block, list, dict, match, iterator, or hash-statement parsing declines, either implement the missing nightly route with tests or reject it with a stable syntax diagnostic; do not rewind into `parse_expr(context)`, `parse_block_only(context)`, `parse_stmt_or_expr_legacy(context)`, or `parse_hash_tag(context)`.
23. Public syntax validation is locked to the hand-written nightly parser. `styio check --syntax --json` may accept `--parser-engine nightly` as an explicit no-op, but non-authoritative engines such as `legacy` or `new` must stay rejected by tests.
24. Temporary ASTs produced during parser failures must stay under local ownership until `ListAST`, `DictAST`, `BlockAST`, `MainBlockAST`, `RangeAST`, parameter-list adopters, or equivalent final AST nodes take them. Session-arena release does not run destructors for abandoned parser nodes, so exception paths that drop partially parsed literals, block statements, or function and iterator parameter lists can leak nested string storage even when the outer AST object memory comes from the arena.
25. Statement-local expression accumulators follow the same rule. Parser helpers such as `parse_print(...)` and nightly statement subsets must keep temporary expression lists behind RAII until `PrintAST` or another final adopter takes ownership, or a malformed outer delimiter can leak inner call-argument buffers after the callee AST has already been constructed.
26. Iterator hash-tag accumulators are part of the same ownership contract. `parse_iterator_tail(...)` and nightly iterator parsing must keep temporary `HashTagNameAST` lists behind RAII until `IterSeqAST` adopts them, or malformed outer expressions can leak completed `#tag` names after the iterator sequence has already been recognized.
27. Iterator continuation and forward-clause recovery must also stay fail-closed under RAII. Once parser fallback has built a collection, guard condition, `?=` right-value list, or iterator body AST, keep it locally owned until `IteratorAST`, `StreamZipAST`, `InfiniteLoopAST`, or `CheckEqualAST` has adopted it, or malformed outer continuations can leak completed nested ASTs after a later delimiter or route check fails.
28. Statement-entry names are part of the same fail-closed contract. In `parse_stmt_or_expr_legacy(...)` and shadow-mode recovery, keep `NameAST`, typed bind targets, and compound-assignment operands behind local ownership until the final bind or `BinOpAST` has adopted them, or malformed right-hand expressions can leak the already-created statement prefix across both legacy and nightly entry routes.
29. `@resource` references belong to the same ownership boundary. `parse_resource_ref_after_at_latest(...)` must keep the parsed `NameAST` behind RAII until `ResourceRefAST` adopts it, or malformed selectors and outer `#...` recovery can leak the completed `@name` prefix across legacy entry, nightly subset recovery, and shadow-mode fallback.
30. Native source references use compatibility `@extern(c|c++) => "relative/or/absolute/source"` and the preferred explicit binding form `# name[, other] := @ extern(c|c++) { "relative/or/absolute/source" }` as parser-owned structure only. The parser may normalize a relative path against the `.styio` file location and store it on `ExternBlockAST`, but syntax-only paths must not open, stat, compile, or validate the referenced file. Inline native bodies in `# name := @ extern(c|c++) { ... }` must remain raw-scanned by the tokenizer, because C/C++ braces, strings, comments, and raw string literals are not Styio syntax.
31. Primitive token-table changes are frontend-owned public surface. When a scalar such as `char` changes its canonical width or type metadata, update the token table together with AST/Sema/IR/codegen tests so syntax acceptance does not drift from executable type behavior.
32. Parser header declarations must stay one canonical prototype per helper. After merge-heavy recovery work, rebuild `styio_frontend_core` or a dependent target and remove duplicate declarations or repeated default arguments before pushing `nightly`.
33. `?|` statement routing must preserve the typed task_await shape `?| task -> value: T` and route non-typed resource-effect operations such as `?| resource_operation | ...` to resource settlement/discard parsing. Do not interpret every `?|` statement as task await, and do not admit resource-effect discard in expression contexts.
34. The current resource-effect parser slice accepts catch-all `?| resource_operation | fallback` as a statement and wraps non-task `?|` operations in `ResourceEffectAST`. Keep `?| resource_operation | effect => handler` and handler chains fail-closed until the typed handler dispatch slice lands; do not parse them as ordinary fallback expressions.

## Change Classes

1. Small: typo-safe parser helper changes, local lookahead fix, or token display-name cleanup. Run targeted unit or feature tests.
2. Medium: new token, changed AST construction, changed parser rejection, or changed parse diagnostics. Add tests and run parser authority/shadow gates for affected feature labels.
3. High: default parser route, parser authority policy, statement boundary, or syntax-check service behavior. Use checkpoint workflow, add ADR if ownership or route policy changes, and run checkpoint health.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build/default -L language_feature
ctest --test-dir build/default -R '^StyioParserEngine\.'
ctest --test-dir build/default -R '^StyioDiagnostics\.SyntaxCheckRejectsNonAuthoritativeParserEngine$'
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

1. Sema / IR must review changes that alter AST shape, node ownership, or parser semantic output.
2. Test Quality must review new parser acceptance fixtures, shadow gate changes, and fuzz regression samples.
3. IDE / LSP and Grammar must review changes that affect edit-time syntax assumptions or public diagnostics.
4. Codegen / Runtime must review syntax changes that add, rename, or reroute `styio_*` runtime helpers.
5. Docs / Ecosystem must review changes to language SSOT links or parser migration workflow docs.

## Handoff / Recovery

Record unfinished parser work in `docs/history/YYYY-MM-DD.md` with:

1. Parser engine, route, and feature subset.
2. Exact failing command or shadow artifact path.
3. Parser-authority and no-fallback status.
4. Next smallest parser slice.
5. Rollback point if accepted syntax changed.
