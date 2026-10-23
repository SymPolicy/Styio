# C0 — Parser Core Unification Record

**Purpose:** Record the frozen accepted-expression parser-core contract for Checkpoint C (`OPT-C`): one canonical `parse_expr` core, one constexpr operator-metadata precedence authority, and the O(n), depth, diagnostic, no-fallback, and complete-migration invariants retained by active inventories and tests.

**Last updated:** 2026-08-02

**Status:** Frozen completed OPT-C parser-owner contract. Current authority and evidence live in IM-D2, owner runbooks, parser tests, and benchmark probes.

## 1. Expression Entry Functions

`parse_expr(StyioContext&)` is the canonical full-expression entry. Nightly subset and delimiter-bounded entries are routing wrappers around the same core, never separate precedence implementations. `StyioContext` remains the only token cursor, diagnostic owner, recovery-mode owner, and optional statistics sink.

## 2. Precedence Authority

One immutable `constexpr` table in `src/StyioToken/Token.hpp` is the only accepted-expression infix precedence, associativity, operator-kind, and AST-operation authority: `StyioExprOperatorInfo`, `StyioExprAssociativity`, `StyioExprOperatorKind`, and the zero-allocation lookup `styio_expr_operator_info(StyioTokenType)`. The lookup returns `nullptr` for a token outside the accepted infix grammar. `StyioOpType` remains an AST operation identity; its numeric declaration order has no parsing meaning. Enum ordinals and hash lookups never decide precedence. The complete-migration contract removes the former `TokenPrecedenceMap` and ordinal-based `parse_binop_rhs` comparison (see §8).

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

## 3. Algorithm and Associativity

The core is an LLVM-style iterative left fold with rust-analyzer explicit binding-power semantics: for an operator with precedence `p`, the RHS minimum is `p + 1` when left-associative and `p` when right-associative. The loop stops without consuming when the token is not an operator, is an allowed follow token for this entry, or has precedence below the current minimum. `**` (power) is the only right-associative infix operator in the accepted grammar; `2 ** 3 ** 2` nests right.

## 4. Unary / Postfix / Call / Index / Resource Suffix

Prefix and postfix ownership stays outside the infix loop. Unary `+` produces no AST node; signed integer/decimal negatives remain literal atoms; and the frozen non-literal unary-minus shape remains `0 - <remaining expression>` within the delimiter boundary. Call/index/slice tails apply left-to-right, and no postfix crosses a line break. Resource-effect suffixes (`-> @resource`, `>> @resource`) own the complete preceding additive AST; the complete-migration contract retains no reassociation helper.

## 5. Parser Fallback / Legacy Bridge Boundary

Once an accepted expression FIRST token is recognized, the nightly parser owns the route. A malformed or unsupported continuation is fatal; it is never rewound into a legacy expression, block, statement, match, list, dict, iterator, or hash parser. Input that is not in expression FIRST stays `Declined` with the byte/token cursor unchanged. Explicitly legacy-only statement/conditional engine code remains intact outside this accepted expression boundary.

## 6. Token API Dependency Status

All `->original` accesses in parser code access NAME, INTEGER, DECIMAL, or STRING tokens — all of which have valid `original` text. No parser code reads `original` from operator tokens, so the B2 span-first tokenizer remains fully backward compatible.

## 7. Frozen Acceptance Matrix

The executable matrix is owned by `tests/parser_internal_test.cpp` (`StyioParserInternal`) and `benchmark/internal/core_bench.cpp` (`expr_flat_add_4096`, `expr_mixed_4096`, `expr_right_power_64`); the active authority summary is [IM-D2](./IM-D2-PARSER-AUTHORITY-INVENTORY.md). The golden expectations that lock the required behavior:

| Input | Expected precedence |
|-------|---------------------|
| `1 + 2 * 3` | `1 + (2 * 3)` = 7 |
| `(1 + 2) * 3` | `3 * 3` = 9 |
| `2 ** 3 ** 2` | Right-assoc: `2 ** (3 ** 2)` = 512 |
| `-2 ** 3` | Signed atom binds tighter than power: `(-2) ** 3` = -8 |
| `a && b \|\| c` | `(a && b) \|\| c` |
| `a == b && c != d` | `(a == b) && (c != d)` |
| `f(x)(y)[0]` | `((f(x))(y))[0]` |
| `xs[0..2]` | Slice |
| `1 + 2 \| fallback` (delimiter-bounded) | `1 + 2` parsed; cursor left on `\|`; no fallback consumption |

Work is O(n) in tokens: `expression_token_visits <= 8 * token_count + 8`, zero expression-core scratch allocations on flat chains, and `kStyioExprMaxDepth` = 128 active expression frames (the 129th fails with `StyioParserResourceLimitError` "expression exceeds parser recursion limit of 128"; existing delimiter nesting remains capped at 64).

## 8. Migration Boundary

The completed parser-core migration is retained as current obligations in [IM-D2](./IM-D2-PARSER-AUTHORITY-INVENTORY.md) and the Frontend runbook:

- Replace the unordered `TokenPrecedenceMap` with the canonical constexpr metadata and lookup; spelling maps stay spelling maps.
- Change `parse_binop_rhs` to numeric minimum-precedence/binding-power semantics; remove all `StyioOpType` ordinal comparisons.
- Remove accepted-path `parse_arithmetic_tail_from_atom`, `parse_relational_expr`, `parse_and_expr`, `parse_or_expr`, `parse_fallback_expr`, and `reassociate_add_into_resource_sink_latest_draft`; update their callers to the common entry without retaining wrappers.
- Remove `expr_prec_of`, `expr_is_right_assoc`, `expr_map_binop`, `expr_is_comp`, `expr_map_comp`, `expr_is_logic`, and `expr_map_logic` from `NewParserExpr.cpp`.
- Remove any nightly call into legacy `parse_expr`, `parse_stmt_or_expr_legacy`, `parse_block_only`, or `parse_hash_tag`; the common expression entry is not a legacy bridge.
- Leave explicitly legacy-only statement/conditional engine code intact; create no compatibility alias or fallback route for removed expression helpers.

The old-symbol absence check was a one-time migration audit; current regressions rely on executable parser authority tests rather than a permanent source-grep test.
