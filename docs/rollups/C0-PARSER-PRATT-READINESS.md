# C0 — Parser Pratt Readiness Inventory

**Purpose:** Record the current parser expression structure and identify migration boundaries for Checkpoint C (Pratt/precedence-climbing parser unification). This is a readiness-only snapshot; no parser rewriting is in scope until C0 is approved.

**Last updated:** 2026-06-24

**Status:** Pre-implementation inventory for Checkpoint C.

## 1. Expression Entry Functions

All locations in `src/StyioParser/Parser.cpp` and `src/StyioParser/NewParserExpr.cpp`.

| Function | File:Line | Role |
|----------|-----------|------|
| `parse_expr` | Parser.cpp:3812 | Top-level expression entry |
| `parse_expr_postfix` | Parser.cpp:3581 | Postfix tails (match, >>, ->, ., call, loop) |
| `parse_arithmetic_tail_from_atom` | Parser.cpp:2909 | Post-atom arithmetic + indexing + call tail |
| `parse_fallback_expr` | Parser.cpp:3228 | `a \| b` fallback/alternative |
| `parse_or_expr` | Parser.cpp:3210 | `a \|\| b` logical or |
| `parse_and_expr` | Parser.cpp:3192 | `a && b` logical and |
| `parse_relational_expr` | Parser.cpp:3141 | `== != > < >= <=` |
| `parse_arithmetic_expr` | Parser.cpp:2996 | Unary +/-, atoms |
| `parse_binop_item` | Parser.cpp:3657 | Atomic binop operand |
| `parse_binop_rhs` | Parser.cpp:4652 | Precedence climbing for + - * / % ** |
| `parse_call` | Parser.cpp:4178 | Function/method call |
| `parse_index_op` | Parser.cpp:4306 | Index a[i], slice a[i:j] |
| `reassociate_add_into_resource_sink_latest_draft` | Parser.cpp:701 | Special + reassoc for resource sinks |
| `try_parse_expr_subset_until_latest` | Parser.cpp:30 | Delimiter-bounded subset parsing |

## 2. Precedence Sources

Two concurrent systems exist:

### A. `TokenPrecedenceMap` (Token.hpp:666)
Numeric precedence table mapping `StyioOpType` → `int`:
- 999: unary +/-, ~, !
- 704: ** (power)
- 703: * / %
- 702: + -
- 701: shl, shr
- 502: > < >= <=
- 501: == !=
- 303: &
- 302: ^
- 301: |
- 203: &&
- 202: o+ (xor)
- 201: ||

### B. Enum ordinal comparison (parse_binop_rhs, line 4660)
`parse_binop_rhs` uses `next_token > curr_token` based on `StyioOpType` enum ORDINAL values, NOT the numeric `TokenPrecedenceMap`. Comment at line 4646: "hi, you need to pass the precedence as a parameter."

**Risk:** Operator precedence in the climbing parser is defined by enum declaration order, not the explicit precedence table. The two can diverge silently.

## 3. Associativity

- `parse_binop_rhs` (line 4660): `if (next_token > curr_token)` → right-associative recursion. Otherwise left-associative via reassociation.
- `**` (power) is the primary right-associative operator.
- Current behavior for `2 ** 3 ** 2` should be verified via golden test.

## 4. Unary / Postfix / Call / Index / Resource Suffix

| Category | Entry Point | File:Line |
|----------|-------------|-----------|
| Unary +/- | `parse_arithmetic_expr` via `parse_binop_item` | Parser.cpp:2996 |
| Unary ! (logic not) | `parse_binop_item` | Parser.cpp:3657 |
| Postfix () call | `parse_arithmetic_tail_from_atom` / `parse_call` | Parser.cpp:2909 / 4178 |
| Postfix [] index | `parse_index_op` | Parser.cpp:4306 |
| Postfix .attr | `parse_arithmetic_tail_from_atom` | Parser.cpp:2909 |
| Postfix match | `parse_expr_postfix` | Parser.cpp:3581 |
| Resource suffix >> @resource | `parse_arithmetic_tail_from_atom` | Parser.cpp:2909 |
| Resource suffix -> @resource | `reassociate_add_into_resource_sink_latest_draft` | Parser.cpp:701 |
| Yield pipe <\| | embedded in expression parsing | multiple |

## 5. Parser Fallback / Legacy Bridge Entries

| Entry | File:Line | Status |
|-------|-----------|--------|
| `parse_stmt_or_expr_legacy` | Parser.cpp | Legacy-only; nightly rejects via `reject_authoritative_nightly_gap_latest` |
| `parse_main_block_legacy` | Parser.cpp | Legacy-only |
| `parse_block_only` | Parser.cpp | Shared |
| `parse_cond` / `parse_cond_rhs` | Parser.cpp:4842/4744 | Legacy conditional parser |
| `parse_name_and_following_unsafe` | Parser.cpp:734 | Name + arithmetic postfix |

## 6. Token API Dependency Status

All `->original` accesses in parser code access NAME, INTEGER, DECIMAL, or STRING tokens — all of which have valid `original` text. No parser code reads `original` from operator tokens. Therefore the B2 span-first tokenizer is fully backward compatible.

**Remaining `->original` sites:** ~30 in Parser.cpp and NewParserExpr.cpp. All safe.

## 7. Precedence Golden Tests

The following golden inputs lock current parser behavior. Run as feature tests or pipeline cases before any Pratt refactoring:

| Input | Expected precedence | Risk |
|-------|-------------------|------|
| `1 + 2 * 3` | `1 + (2 * 3)` = 7 | Low |
| `(1 + 2) * 3` | `3 * 3` = 9 | Low |
| `2 ** 3 ** 2` | Right-assoc: `2 ** (3 ** 2)` = 512 | **Verify** |
| `-2 ** 3` | Unary binds tighter: `(-2) ** 3` = -8 | **Verify** |
| `a && b \|\| c` | `(a && b) \|\| c` | Low |
| `a == b && c != d` | `(a == b) && (c != d)` | Low |
| `f(x)(y)[0]` | `((f(x))(y))[0]` | Medium |
| `xs[0..2]` | Slice | Low |
| `?\|\ job -> value: i64 \| 0` | Await pipe | Low |
| `@stdin & xs >> #(line, x) => { ... }` | Stream zip | Medium |

## 8. Checkpoint C Migration Boundary

### Things to keep:
- `TokenPrecedenceMap` as the authoritative precedence table
- `parse_binop_item` as the atom parser
- `parse_call`, `parse_index_op` as postfix parsers
- Nightly parser as the only accepted authority

### Things to replace:
- `parse_binop_rhs` enum-ordinal comparison → numeric precedence from `TokenPrecedenceMap`
- `parse_relational_expr` / `parse_and_expr` / `parse_or_expr` / `parse_fallback_expr` recursive descent chain → unified Pratt table
- `reassociate_add_into_resource_sink_latest_draft` → precedence-aware resource suffix
- `parse_arithmetic_tail_from_atom` → postfix loop inside Pratt

### Things to add:
- Explicit precedence table (constexpr array of `{token, precedence, associativity}`)
- FIRST/FOLLOW or sync-point table for error recovery
- Parser benchmark harness

### Legacy fallback contract:
- `parse_cond`, `parse_cond_rhs`, `parse_stmt_or_expr_legacy` remain for legacy parser engine only
- Nightly parser must not fall through to legacy for any accepted grammar form
