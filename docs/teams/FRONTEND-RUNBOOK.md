# Frontend Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of Styio tokenization, parsing, Unicode handling, and the authoritative nightly parser contract; this file links to language and test SSOTs instead of redefining grammar.

**Last updated:** 2026-08-10

## Mission

Own the source-to-AST front end: token definitions, lexer behavior, parser routing, parser diagnostics, lookahead helpers, and the authoritative nightly parser boundary. Do not own language meaning beyond implementing the design SSOT.

Delimiter nesting queries use a parser-owned prefix table built once per token stream; operation-count tests protect linear construction and constant-time lookup without changing accepted syntax or diagnostics.

## Owned Surface

Primary paths:

1. `src/StyioToken/`
2. `src/StyioUnicode/`
3. `src/StyioParser/`
4. Git history only when a deleted parser snapshot is needed for migration reference
5. Parser-facing tests under `tests/features/`, `tests/fuzz/`, and parser shadow gates
6. Parser legacy-entry audit scripts: `scripts/parser-legacy-entry-audit.py` and `scripts/parser-legacy-entry-audit.sh`

Build and test targets:

1. `styio_frontend_core`
2. `styio_core`
3. `styio_test`
4. `styio_fuzz_lexer` and `styio_fuzz_parser` when fuzz is enabled

## Daily Workflow

1. Read [../design/Styio-EBNF.md](../design/Styio-EBNF.md), [../design/Styio-Symbol-Reference.md](../design/Styio-Symbol-Reference.md), and relevant language sections before changing syntax.
2. Check [../rollups/CURRENT-STATE.md](../rollups/CURRENT-STATE.md), [../rollups/NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md), [../rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md](../rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md), [../rollups/C0-PARSER-PRATT-READINESS.md](../rollups/C0-PARSER-PRATT-READINESS.md), and the parser gate sections in [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md) when touching parser authority paths; use Git history only if active docs are still insufficient.
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
13. Task launch and await syntax is symbol-only: `||> { ... }` constructs one task block, `||> [ t1 := { ... } t2 := { ... } ]` launches a task group, and `?| job -> answer: T | fallback` awaits a task/future handle into a newly declared local. `?| resource_operation | fallback` is also the only resource fallback surface; `?| resource_operation | effect => handler` handles only that typed effect family, such as `backpressure`, and handler chains are allowed. Bare `| fallback` is not resource fallback. `?| op | ...` is accepted only as a standalone discard statement that produces no value; parsers must reject it in assignment, call-argument, branch-value, resource-method return-value, and other expression contexts. Statement-shaped file handle acquire/rebind, direct file iterators, direct file release, and statement-shaped resource method calls parse through the resource-effect route, but Sema must still prove the resource family so ordinary scalar binds, non-file iterators, and member calls do not become resource effects. Value-producing resource-effect expressions are currently limited to explicit non-task expression routes such as file/acquired-handle instant pulls, stdin pulls, materialized list/dict/matrix index or slice reads, and resource method calls whose user-defined body is a Sema-accepted single `<| expr` return, statement-only preface plus final `<| expr`, or scalar/local list/dict/matrix `=` / `:=` prefaces plus final scalar/string or local list/dict/matrix container `<| expr`. Parsers must mark the AST as value-required so Sema can reject statement-shaped operations, non-file acquired-handle pull sources, expression discard, statement-only called-function returns, local resource binding, capture-dependent method bodies, and unsupported method bodies. `?| op | effect => @()` must still be rejected because `@()` is not an executable empty action. Without fallback, `?| job -> answer: T` or `?| resource_operation` settles in place and raises immediately on failure. Bare `?| -> answer: T` is a reserved continuation freeze shape and must remain fail-closed until continuation lowering exists.
14. Function-level match sugar `# name := (single_param: T) ?= { ... }` is parser-local normalization only: require exactly one parameter and construct the same `MatchCasesAST(NameAST(param), cases)` body that explicit match syntax would feed to lowering.
15. Retired state-resource state families are parser errors. Keep `@name : Type`, `expr -> @name`, `@name[-1]`, and standalone `(<< @file(...))` expression routes green in both parser routes where shadow compatibility still applies, and do not let postfix parsing cross a line break into the next statement.
16. Resource method syntax is parser-owned but sema-resolved: accept `@file::name = ...`, `@file::name := ...`, compatible `@file.name = ...` definitions, `@("path")` file-resource expressions, `@()` empty-resource sinks, and expression postfix calls such as `@("path").close()`. Inside a resource method body, bare `@file` and postfix forms such as `@file.dispose()` or `@file.path` are the receiver instance for that method family, not a constructor. Any expression parser path still used by resource method bodies must admit accepted scalar literals such as single-quoted `char` through the same token-level rules as ordinary expressions, plus canonical parenthesized stdin instant pulls such as `(<- @stdin)`, receiver-scoped property postfix in property and method definitions, accepted materialized container tails such as list index/list slice, inline `dict{...}` literal single-key index, ordered dict value slice, typed matrix-parameter cell/row or row-range slice reads, scalar/string match expressions, ordinary function calls whose called function has a value-producing tail, statement-only prefaces before a final returned expression, scalar local `=` / `:=` prefaces or local list/dict/matrix `=` / `:=` prefaces before a final scalar/string or local list/dict/matrix container returned expression, and value-producing resource-effect expressions when a single `<| expr` method body returns the value; adjacent invalid literals such as multi-byte chars, malformed format strings, top-level `@file.path` shorthand, returned container match results, statement-only called-function returns, local resource binding method bodies, expression discard, and unimplemented lexical/global matrix captures must fail closed instead of falling through to a compatibility route.
17. Typed stdin pull syntax is one parser route: `name[, name...] <- @stdin : T-or-(T, ...)`. Scalar pulls and single-target collection pulls such as `xs <- @stdin : list[i64]` must both lower through typed `InstantPull`.
18. `$` remains syntax-sensitive: `$"..."` is the accepted format-string route and must stay green in both parser engines, while `$identifier` is retired state-resource state syntax and must keep the migration diagnostic.
19. Tokenizer and parser recovery paths are sanitizer-sensitive. Accumulate tokens, top-level statements, hash-function parts, parsed return-type fragments, parenthesized expressions, call arguments, and match-case arms/default bodies behind RAII ownership before releasing them to the session or final AST node, and backflow minimized fuzz samples into `tests/fuzz/corpus/` when nightly fuzz exposes a lifetime bug.
20. Typed annotation recovery is part of the same ownership contract. Keep parsed `TypeAST`, declared `VarAST`, await targets, resource declaration slots, and parameter nodes behind local owners until the parser has seen the required delimiter or assignment token and the final AST node has adopted them.
21. Parser resource limits are fail-closed. Deep delimiter nesting and unclosed expression contexts must raise parser diagnostics directly instead of being swallowed by legacy fallback; minimized OOM fuzz seeds belong in `tests/fuzz/corpus/` with a deterministic security regression.
22. `StyioToken::length()` and `StyioToken::as_str()` use `lexeme()`/`textString()` instead of `original` (commit 8117705). The `original` field is deprecated and empty for all tokenizer-produced zero-copy span tokens. When new token facade methods are added, they must use `lexeme()` or `textString()` instead of `original`.
22. Accepted grammar is no-fallback. When nightly expression, statement, block, list, dict, match, iterator, or hash-statement parsing declines, either implement the missing nightly route with tests or reject it with a stable syntax diagnostic; do not rewind into `parse_expr(context)`, `parse_block_only(context)`, `parse_stmt_or_expr_legacy(context)`, or `parse_hash_tag(context)`. Retired `filter` / `slice` CODP spellings, legacy value-position `|expr|`, malformed `|name` size probes, and unsupported indexed or call postfixes after var-name/value-expression routes must fail closed instead of falling into legacy iterator or index parsing.
23. Public syntax validation is locked to the hand-written nightly parser. `styio check --syntax --json` may accept `--parser-engine nightly` as an explicit no-op, but non-authoritative engines such as `legacy` or `new` must stay rejected by tests.
24. Temporary ASTs produced during parser failures must stay under local ownership until `ListAST`, `DictAST`, `BlockAST`, `MainBlockAST`, `RangeAST`, parameter-list adopters, or equivalent final AST nodes take them. Session-arena release does not run destructors for abandoned parser nodes, so exception paths that drop partially parsed literals, block statements, or function and iterator parameter lists can leak nested string storage even when the outer AST object memory comes from the arena.
25. Statement-local expression accumulators follow the same rule. Parser helpers such as `parse_print(...)` and nightly statement subsets must keep temporary expression lists behind RAII until `PrintAST` or another final adopter takes ownership, or a malformed outer delimiter can leak inner call-argument buffers after the callee AST has already been constructed.
26. Iterator hash-tag accumulators are part of the same ownership contract. `parse_iterator_tail(...)` and nightly iterator parsing must keep temporary `HashTagNameAST` lists behind RAII until `IterSeqAST` adopts them, or malformed outer expressions can leak completed `#tag` names after the iterator sequence has already been recognized.
27. Iterator continuation and forward-clause recovery must also stay fail-closed under RAII. Once parser fallback has built a collection, guard condition, `?=` right-value list, or iterator body AST, keep it locally owned until `IteratorAST`, `StreamZipAST`, `InfiniteLoopAST`, or `CheckEqualAST` has adopted it, or malformed outer continuations can leak completed nested ASTs after a later delimiter or route check fails.
28. Statement-entry names are part of the same fail-closed contract. In `parse_stmt_or_expr_legacy(...)` and shadow-mode recovery, keep `NameAST`, typed bind targets, and compound-assignment operands behind local ownership until the final bind or `BinOpAST` has adopted them, or malformed right-hand expressions can leak the already-created statement prefix across both legacy and nightly entry routes.
29. `@resource` references belong to the same ownership boundary. `parse_resource_ref_after_at_latest(...)` must keep the parsed `NameAST` behind RAII until `ResourceRefAST` adopts it, or malformed selectors and outer `#...` recovery can leak the completed `@name` prefix across legacy entry, nightly subset recovery, and shadow-mode fallback.
30. Native source references use compatibility `@extern(c|c++) => "relative/or/absolute/source"` and the preferred explicit binding form `# name[, other] := @ extern(c|c++) { "relative/or/absolute/source" }` as parser-owned structure only. The parser may normalize a relative path against the `.styio` file location and store it on `ExternBlockAST`, but syntax-only paths must not open, stat, compile, or validate the referenced file. Inline native bodies in `# name := @ extern(c|c++) { ... }` must remain raw-scanned by the tokenizer, because C/C++ braces, strings, comments, and raw string literals are not Styio syntax.
31. Primitive token-table changes are frontend-owned public surface. When a scalar such as `char` changes its canonical width, literal spelling, or type metadata, update the token table and every accepted expression subset route together with AST/Sema/IR/codegen tests so syntax acceptance does not drift from executable type behavior. Single-quoted char literals and token-level format strings must stay on the nightly parser route and any accepted resource-method body parser path, and continue to produce `CharAST` / `FmtStrAST`.
32. Parser header declarations must stay one canonical prototype per helper. After merge-heavy recovery work, rebuild `styio_frontend_core` or a dependent target and remove duplicate declarations or repeated default arguments before pushing `nightly`.
33. `?|` statement routing must preserve the typed task_await shape `?| task -> value: T` and route non-typed resource-effect operations such as `?| resource_operation | ...` to resource settlement/discard parsing. Do not interpret every `?|` statement as task await, and do not admit resource-effect discard in expression contexts.
34. The current resource-effect parser slice accepts catch-all `?| resource_operation | fallback`, `?| resource_operation | effect => handler`, handler chains, statement-shaped file handle acquire, statement-shaped file rebind, statement-shaped direct file iterators, statement-shaped resource method calls, acquired file-handle instant pulls such as `?| (<< f) | fallback`, materialized list/dict/matrix index value expressions, list-slice and ordered dict value-slice expressions, matrix row and row-range value expressions, and a final catch-all fallback after named handlers as `ResourceEffectAST`. Statement parsing keeps the value-required flag off so statement-shaped fallbacks do not become `main` return values; expression parsing sets the flag for value-producing uses such as file instant-pull fallback, acquired-handle file instant-pull fallback, untyped stdin instant-pull fallback, explicit-target typed stdin fallback, `?| xs[i] | fallback` list-index recovery, `?| xs[0..] | fallback` list-slice recovery, `?| d[key] | fallback` dict-index recovery, `?| d[0..] | fallback` ordered dict value-slice recovery, `?| m[row][col] | fallback` matrix-index recovery, `?| m[0..2] | fallback` matrix row-range recovery, and resource-method value calls such as `?| log.answer() | fallback`, `?| log.read_stdin() | fallback`, `?| log.cell(m) | fallback`, `?| log.rows(m) | fallback`, or methods that return a value-producing `ResourceEffectAST` when Sema proves a single-return or accepted preface method body and declared parameter types. Keep handler bodies parsed as executable resource code that stops before the next handler-chain `|`, keep `?| resource_operation | effect => ...` rejected, keep `?| resource_operation | ...` rejected in expression and resource-method-return positions, do not parse named handlers as ordinary fallback expressions, and leave unsupported statement-shaped acquire/rebind sources, non-file iterator collections, non-file acquired-handle instant-pull names, non-resource member calls, unsupported method bodies, and unsupported lexical/global captures to Sema instead of adding parser fallback.
35. `name << @resource[-n..]` and `name << @resource[...]` are explicit selector-copy forms for bounded resource topology snapshots and should parse to a binding over the selector value, not a resource write target. Keep `name << @resource[-1]` fail-closed because it is a scalar latest read, and preserve existing file/standard-stream `value << @file(...)` write compatibility.
36. Zip right-hand collection parsing must stop before the right-hand `>>` separator for accepted collection expressions such as bound names. Do not let the expression subset consume `name >>` as an unsupported continuation before `StreamZipAST` can own the right collection, parameters, and body.
37. Zip `@` collection parsing must distinguish resource atoms from named resource selectors. `@file(...)`, `@{...}`, and `@stdin` stay resource atoms for accepted file/stdin zip, `@stdout` and `@stderr` must still fail semantically as non-iterable zip inputs, while `@name[-n..]` and `@name[...]` enter the zip collection slot as selector values that Sema must prove iterable. Do not parse right-hand resource collections as full statements or postfix expressions that can consume the iterator continuation.
38. Resource-selector iterators at statement start, such as `@price[...] >> #(p) => { ... }`, must route through the nightly iterator tail without crossing a line break or legacy fallback. Scalar latest selectors such as `@price[-1] >> ...` may parse, but Sema must keep them rejected as non-iterable inputs; public JSONL diagnostics for that boundary use `STYIO_TYPE_ITERATION_UNSUPPORTED_SOURCE` without implying broader selector or stream-driver support. Duplicate `@stdin & @stdin` zip may parse through the accepted zip surface, but Sema must keep it rejected with `STYIO_TYPE_STREAM_DUPLICATE_DRIVER_UNSUPPORTED` until a duplicate external-input driver contract exists.
39. Materialized ranges in the nightly list route accept `[start..end]` with integer expression operands and lower through `RangeAST` with an internal unit step. Keep parsed start/end expressions under RAII ownership until `RangeAST` adopts them, reject reserved `[start..end..step]` before Sema, and do not route `[start..end]` through ordinary `ListAST` as a single range-expression element or through legacy parser fallback.
40. Pressure observer syntax is a narrow parser/Sema boundary: `resource.pressure >> #(p) => { ... }` may parse through the nightly attribute/iterator path so Sema can issue `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED` for every current resource family. Keep ordinary unsupported dot-expression `>>` continuations rejected, do not add legacy fallback for pressure observers, and do not treat this parser route as pressure payload typing or runtime observer execution.
41. Tokenizer uses span-first scanning with O(n) linear complexity. `StyioTokenizer::tokenize(const std::string& code)` records source spans (`begin()`, `len()`) in every token via `CreateFromSpan`. The `original` std::string field is built from the span in a single allocation — never char-by-char. Operator matching uses a constexpr lookup table with longest-match precedence. Parser code may continue to access `token->original` for owned text or `token->lexeme()` for a zero-copy `std::string_view` into the source buffer (caller must keep the source alive). New tokenizer features must: (a) use `CreateFromSpan` in the hot path, not `Create` with a pre-built string; (b) add operator entries to `kOperatorTable` sorted length-descending per first character; (c) keep `token->original` populated for diagnostic/parser backward compatibility; (d) add span-correctness and perf regression tests under `StyioTokenizerSpan` / `StyioTokenizerPerf`.
42. Parser engine dispatch is fail-closed. Unknown `StyioParserEngine` values must stringify as `invalid` and throw `StyioParseError` at every parser dispatch boundary, including top-level parsing and format-string embedded-expression parsing. Never map an invalid enum to `legacy`, `nightly`, `new`, or any compatibility route.

43. `StyioDataType` canonical field changes are frontend-owned because token/type metadata is shared by Sema and session services. Keep `StyioDataType::equals()` and any canonical view used by `TypeTable` in lockstep so parser-produced primitive, stream, collection, and resource type metadata compare through the same field set.

44. Continue spelling is token-width tolerant but semantically depthless. Tokenizers may preserve the full `>>...` lexeme for diagnostics and highlighting, while parser routes must construct the same `ContinueAST` for every standalone continue spelling.
45. Hash callable binding parsing owns the source-level distinction between callable endpoints and resources. Accept `# name = (...) => ...`, `# name := (...) => ...`, and the explicit callable-body marker `# name = #(args) => ...`; reject direct resource atoms such as `# sink = @stdout` in the hash binding route so resource identities stay visibly in the `@` family. Native `@ extern(...)` binding remains its separate parser-owned import form.
46. Styio has no word-token keywords. Tokenize every identifier-shaped word as `NAME`; inspect exact spellings only after punctuation or an already-selected structural production supplies the context. Keep keyword-like ordinary bindings executable and keep Boolean literal spellings lexically classified as `NAME`.
47. Callable type syntax is a recursive type-position form, `#(T1, T2): R`, with one whitespace-free canonical identity stored in `StyioDataType`. Parse nested parameter and result callable types through the normal type parser and preserve right-side result precedence: in `#(i64): i64..`, the repetition suffix belongs to the result type, not the callable value. Do not add a second signature representation in the parser or reinterpret value-position `#` declarations as type syntax.
48. Explicit callable capture syntax is `$(name, ...)` after the declared parameter/result signature and before the callable binding/body operator, for example `# add : i64 $(seed) := (value: i64) => value + seed`. The capture list must be nonempty and duplicate-free, must remain attached to `FunctionAST` / `SimpleFuncAST`, and must be recognized by the nightly hash-function route before generic expression fallback. Exact free-name, ownership, and escape validation belongs to Sema; the parser must not infer or silently add captures.
49. The accepted expression parser has one core and one precedence authority. `parse_expr(StyioContext&)` is the canonical full-expression entry; nightly subset and delimiter-bounded entries are routing wrappers around it, never separate precedence implementations. `src/StyioToken/Token.hpp` owns `StyioExprOperatorInfo`, `StyioExprAssociativity`, `StyioExprOperatorKind`, and the constexpr `styio_expr_operator_info(StyioTokenType)` lookup — the only accepted-expression infix precedence, associativity, operator-kind, and AST-operation authority. Enum ordinals and hash lookups never decide precedence; `StyioOpType` is an AST operation identity only. The core is an LLVM-style iterative left fold with rust-analyzer binding-power semantics: the RHS minimum is `p + 1` for left-associative and `p` for right-associative operators, and the loop stops without consuming on a non-operator token, an allowed follow token, or a lower-precedence token. Work is O(n) in tokens with `expression_token_visits <= 8 * token_count + 8`, zero expression-core scratch allocations on flat chains, and `kStyioExprMaxDepth` = 128 active expression frames; the 129th fails with `StyioParserResourceLimitError` ("expression exceeds parser recursion limit of 128") while existing delimiter nesting stays capped at 64. Once an accepted expression FIRST token is recognized, the nightly parser owns the route: malformed or unsupported continuations are fatal and never rewind into legacy expression, block, statement, match, list, dict, iterator, or hash parsers; accepted AST shapes, line-break boundaries, resource-effect delimiter ownership, and stable nightly diagnostics remain unchanged. Ordinary index/slice tails stay in the unified postfix loop, while `avg`/`max`, guard, and equality-probe selectors route through the single named `parse_index_suffix_nightly` seam so special selector ownership cannot drift from legacy entry callers.

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
ctest --test-dir build -R '^StyioParserInternal\.' --output-on-failure
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
