# Parser Core Unification Architecture

**Purpose:** Freeze OPT-C's accepted expression-parser core before production implementation.

**Last updated:** 2026-08-02

**Status:** Frozen for `OPT-C-IMPLEMENT` and `OPT-C-DOCS`.

## Scope and invariants

OPT-C changes only the compiler-owned expression parser, token operator metadata, direct parser acceptance, benchmark probes, and the active parser-owner documentation. It does not add syntax, change Sema/lowering/runtime behavior, alter the legacy engine's authority status, or enter OPT-D/OPT-E.

The delivered boundary has these invariants:

1. `parse_expr(StyioContext&)` remains the canonical full-expression entry. Nightly subset and delimiter-bounded entries are routing wrappers around the same core, not separate precedence implementations.
2. `StyioContext` remains the only token cursor, diagnostic owner, recovery-mode owner, and optional statistics sink.
3. One immutable `constexpr` table in `src/StyioToken/Token.hpp` is the only accepted-expression infix precedence, associativity, operator-kind, and AST-operation authority. Enum ordinals and hash lookup never decide precedence.
4. Once an accepted expression FIRST token is recognized, the nightly parser owns the route. A malformed or unsupported continuation is fatal; it is never rewound into a legacy expression, block, statement, match, list, dict, iterator, or hash parser.
5. Existing accepted AST shapes, line-break boundaries, resource-effect delimiter ownership, and stable nightly diagnostic messages remain unchanged.

## Algorithm decision

### Compared implementations

| Design | Useful property | Limit to carry into Styio |
|---|---|---|
| LLVM Clang `ParseRHSOfBinaryExpression` | Parses one leaf, loops while the next precedence meets `MinPrec`, folds left-associative chains iteratively, and recurses only when a tighter or equal-right-associative RHS must be completed first. | Clang's semantic actions and C/C++ recovery are not Styio dependencies. |
| rust-analyzer `expr_bp` | Uses a direct token-to-binding-power/associativity decision, stops when `op_bp < bp`, passes `bp + 1` for left association and `bp` for right association, and keeps prefix/postfix ownership outside the infix loop. | Its event-marker tree builder and Rust-specific recovery sets are not copied. |
| Existing direct Styio code | Already has the required AST constructors and context-sensitive resource/statement routes. | Ordinal arithmetic precedence, the private nightly switch table, recursive-descent logical/fallback layers, and resource-sink reassociation are duplicate authorities. |

Selected design: one ordinary table lookup plus an LLVM-style iterative left fold using rust-analyzer's explicit binding-power rule. No parselet registry, algorithm hierarchy, callback chain, or dynamic operator map is introduced. For an operator with precedence `p`, the RHS minimum is `p + 1` when left-associative and `p` when right-associative. The loop stops without consuming when the token is not an operator, is an allowed follow token for this entry, or has precedence below the current minimum.

Primary sources used for the comparison:

- [LLVM Clang expression parser](https://github.com/llvm/llvm-project/blob/main/clang/lib/Parse/ParseExpr.cpp)
- [rust-analyzer expression Pratt parser](https://github.com/rust-lang/rust-analyzer/blob/master/crates/parser/src/grammar/expressions.rs)

## Canonical operator metadata

`src/StyioToken/Token.hpp` owns `StyioExprOperatorInfo`, `StyioExprAssociativity`, `StyioExprOperatorKind`, and a zero-allocation `constexpr` lookup named `styio_expr_operator_info(StyioTokenType)`. The lookup returns `nullptr` for a token outside the currently accepted infix grammar.

The table is exact; aliases on one row have identical metadata.

| Token(s) | Precedence | Association | Kind / AST construction |
|---|---:|---|---|
| `YIELD_PIPE` (`<|` in apply position) | 10 | left | apply; `FuncCallAST` |
| `TOK_PIPE` (`|`) | 20 | left | fallback; `FallbackAST` unless `|` is an allowed follow token |
| `LOGIC_OR` | 30 | left | logic; `CondAST(OR)` |
| `LOGIC_AND` | 40 | left | logic; `CondAST(AND)` |
| `BINOP_EQ`, `BINOP_NE` | 50 | left | comparison; `BinCompAST(EQ/NE)` |
| `BINOP_GT`, `TOK_RANGBRAC`, `BINOP_GE`, `BINOP_LT`, `TOK_LANGBRAC`, `BINOP_LE` | 60 | left | comparison; `BinCompAST(GT/GE/LT/LE)` |
| `TOK_PLUS`, `TOK_MINUS` | 70 | left | arithmetic; `BinOpAST(Add/Sub)` |
| `TOK_STAR`, `TOK_SLASH`, `TOK_PERCENT` | 80 | left | arithmetic; `BinOpAST(Mul/Div/Mod)` |
| `BINOP_POW` | 90 | right | arithmetic; `BinOpAST(Pow)` |

Bitwise operators, logical xor/not, shifts, assignments, ranges, match, iterator, and resource arrows are deliberately absent because OPT-C may not broaden accepted syntax. `StyioOpType` remains an AST operation identity; its numeric declaration order has no parsing meaning.

## Expression phases and boundaries

One core invocation executes these phases in order:

1. **FIRST and prefix:** reject or decline before consumption according to the entry contract. Signed integer/decimal negatives stay literal atoms, so `-1 + 2` remains `(-1) + 2` and `-2 ** 3` remains `(-2) ** 3`. The observed non-literal unary-minus behavior is also frozen: `-name * 2` remains `0 - (name * 2)` within the current delimiter boundary. Unary plus produces no AST node. Prefix recursion counts against the expression-depth budget.
2. **Primary:** use the existing accepted nightly atoms only: scalar/char/format literals, names and calls, parenthesized/tuple/list/dict forms, accepted `@` resource atoms/selectors/receivers, guards, task/resource-effect forms, and existing instant-pull forms. OPT-C neither admits a new primary nor changes its diagnostics.
3. **Tight postfix:** consume call `()`, index/slice `[]`, and attribute/member `.` left-to-right without crossing a line break. This phase binds tighter than every infix operator. Existing dot-chain restrictions and messages remain exact.
4. **Infix loop:** consult only `styio_expr_operator_info`, apply the table above, and retain every intermediate AST behind `std::unique_ptr` until its parent adopts it.
5. **Extended suffix:** after the complete infix expression, apply the existing match `?=`, iterator/resource-write `>>`, redirect/flow-bind `->`, and infinite-loop continuations. These context-sensitive suffixes are not table operators. Arithmetic is already inside their data operand, so no AST `dynamic_cast` reassociation is permitted.
6. **FOLLOW:** a caller-supplied, non-owning fixed follow set may stop the core at delimiters such as `|`, comma, closing bracket/parenthesis/brace, or iterator separator. A follow token wins over the infix table. This preserves resource-effect handler/fallback ownership and zip/collection boundaries.

`parse_expr_subset_nightly`, `try_parse_expr_subset_until_latest`, and allowed-follow helpers may remain as named route boundaries, but they must delegate immediately to the common core and contain no precedence, associativity, operator mapping, or postfix implementation.

## Recovery and diagnostics

1. An unrecognized FIRST token returns `Declined` with the cursor unchanged only from a `try_` entry.
2. After a recognized FIRST token, success or `Fatal` are the only outcomes. On `Fatal`, the wrapper restores its saved cursor for caller ownership but preserves the original exception and diagnostic anchor; it never performs a support pre-scan or tries another parser.
3. Strict mode throws the existing `StyioSyntaxError`, `StyioParseError`, or `StyioParserResourceLimitError`. Recovery mode records that same message/range once, then the existing top-level recovery owner synchronizes to the next statement boundary at the starting delimiter depth.
4. Frozen nightly message payloads include `unexpected token in nightly parser expression subset`, `unsupported expression continuation in nightly parser subset`, `expected name after '.' in nightly parser subset`, `dot-chain after call remains unsupported in nightly parser subset`, and the existing delimiter/resource-limit messages. OPT-C does not rewrite them into generic errors.
5. No temporary AST is released before the next required delimiter, operator RHS, or suffix target is validated.

## Bounded work and evidence counters

The core performs O(n) token work for n tokens in the bounded expression. It does not call `expr_subset_route_supported_until_latest` or any other full-expression support scan before parsing. Left-associative flat chains are iterative.

`StyioParserRouteStats` gains optional, zero-initialized counters, updated only when a stats sink is attached:

- `expression_token_visits`: core token classifications, including repeated boundary probes;
- `expression_operator_probes`: canonical table lookups;
- `expression_ast_nodes`: AST nodes created by the common expression core;
- `expression_scratch_allocations`: parser-owned dynamic scratch allocations, excluding adopted AST nodes and pre-existing suffix argument containers;
- `expression_max_depth`: peak active prefix/parenthesis/RHS frames.

For a flat arithmetic chain, `expression_scratch_allocations` is zero, AST nodes are linear, and executable acceptance requires token visits no greater than `8 * token_count + 8`. These counters are evidence only and do not select behavior.

`kStyioExprMaxDepth` is 128 active expression frames. The 129th prefix, parenthesized expression, or right-associative RHS fails before recursion with `StyioParserResourceLimitError` containing `expression exceeds parser recursion limit of 128`. Existing delimiter nesting remains capped at 64. A flat chain of at least 4096 left-associative operators must succeed without approaching the recursion limit.

## Complete migration and removal obligations

`OPT-C-IMPLEMENT` completes the migration in one change:

1. Replace the unordered `TokenPrecedenceMap` with the canonical constexpr metadata and lookup. Keep spelling maps only as spelling maps.
2. Change `parse_binop_rhs` to numeric minimum-precedence/binding-power semantics; remove all `StyioOpType` ordinal comparisons.
3. Remove the accepted-path implementations of `parse_arithmetic_tail_from_atom`, `parse_relational_expr`, `parse_and_expr`, `parse_or_expr`, `parse_fallback_expr`, and `reassociate_add_into_resource_sink_latest_draft`; update their callers to the common entry rather than retaining wrappers.
4. Remove `expr_prec_of`, `expr_is_right_assoc`, `expr_map_binop`, `expr_is_comp`, `expr_map_comp`, `expr_is_logic`, and `expr_map_logic` from `NewParserExpr.cpp`.
5. Remove any nightly call from `NewParserExpr.cpp` into legacy `parse_expr`, `parse_stmt_or_expr_legacy`, `parse_block_only`, or `parse_hash_tag`. The common expression entry is not counted as a legacy bridge.
6. Leave explicitly legacy-only statement/conditional engine code outside this accepted expression boundary intact; do not create a compatibility alias or fallback route for removed expression helpers.

The old-symbol absence check is a one-time migration audit in `Validation.md`, not a permanent source-grep test.

## Cross-node handoff

After this design Node completes, these implementation Nodes are one parallel frontier:

1. `OPT-C-IMPLEMENT` exclusively owns `src/StyioParser/Parser.cpp`, `src/StyioParser/Parser.hpp`, `src/StyioParser/NewParserExpr.cpp`, and `src/StyioToken/Token.hpp`. It consumes this algorithm, the metadata contract, and the frozen executable tests/benchmarks. Its focused proof must leave accepted nightly parsing at zero legacy fallback and zero internal legacy bridges.
2. `OPT-C-DOCS` exclusively owns the three parser rollups, `docs/teams/FRONTEND-RUNBOOK.md`, and `workflows/TEST-CATALOG.md`. It records this already-frozen contract, removes superseded split-authority/readiness wording, and does not infer implementation details beyond this artifact.
3. Neither implementation Node depends on the other's files or output. Both depend only on this design Node.
4. `OPT-C-VALIDATE` starts after both Nodes and their Verifiers complete. One group Reviewer examines the combined parser/token/tests/benchmark/docs boundary, then the frozen impacted regression runs once.

## design_pattern_assessment

```text
pattern_catalog: refactoring-guru-catalog-22-v1
candidate: none
decision: reject
pressure: Two accepted-path expression implementations, ordinal precedence, context-sensitive suffixes, and recovery rewinds can silently diverge while parser hot-path work must remain linear and allocation-bounded.
expected_benefit: No catalog pattern provides a benefit beyond one constexpr value table plus direct functions. The selected direct structure makes precedence/associativity replaceable as data, removes duplicate scans and maps, and is proved by metadata, AST-shape, counter, shadow, and removal seams.
simpler_alternative: A direct Pratt/precedence-climbing loop, an ordinary immutable metadata array, small boundary parameters, and existing StyioContext ownership are sufficient and are the selected solution.
application: Group and OPT-C-IMPLEMENT use the direct table and functions described above; OPT-C-DOCS edits existing SSOTs directly. Strategy is rejected because only one algorithm may remain; Chain of Responsibility is rejected because operator order is fixed data; Adapter is rejected because removed parser cores must not survive behind compatibility wrappers; State is rejected because minimum binding power is ordinary local state; Flyweight is rejected because constexpr metadata already shares immutable values without object identity or a cache.
costs_and_rejections: The direct solution adds one metadata struct/table, one lookup, bounded statistics, and a recursion guard. A Strategy/Template hierarchy would preserve algorithm variation that the task removes; parselet Commands or a responsibility chain would allocate/indirect hot-path work; Facade/Adapter layers would hide migration residue; Flyweight or Singleton would add global lifecycle and cache concerns without measured benefit.
```
