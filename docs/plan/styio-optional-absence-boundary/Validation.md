# Styio Optional Absence Boundary Validation

**Purpose:** Map each Optional/fallback-boundary requirement to executable,
structural, migration, and documentation evidence.

**Last updated:** 2026-07-16

## Validation principles

1. Every negative value-pipe case is paired with positive type, guard, and
   settlement pipe cases so rejection cannot be implemented as blanket token removal.
2. Parser rejection alone is insufficient: structural checks prove that no
   fallback AST/IR/codegen route or `??` token remains.
3. Optional presence is tested with complete payload domains, including
   `i64::MIN`, zero, `false`, empty text, Unit, handles, and containers.
4. Tests are classified by owning grammar/AST type. Accepted resource/effect
   fallback tests remain even though their names contain “Fallback”.

## Requirement matrix

| Requirement | Positive evidence | Negative/static evidence |
|---|---|---|
| `REQ-OA-001` | Ordinary scalar/container/handle values and corresponding `? | T` values type-check with disjoint domains. | Ordinary `T` rejects `(?)`; no arithmetic, logic, equality, or storage path treats a payload bit pattern as absence. |
| `REQ-OA-002` | `(?)`, `[?]`, and `{?}` produce the same empty value under expected `? | T`; ordinary `T` injects the present branch once. | An underdetermined empty literal fails type inference; no bare `@`, `NoneAST`, or `UndefinedLitAST` absence route survives. |
| `REQ-OA-003` | Type interning proves `? | (? | T)` and `? | T` have the same canonical `TypeId` and diagnostic spelling. | No nested Optional tag/layout is created by inference, joins, substitution, or lowering. |
| `REQ-OA-004` | Empty/present round trips cover `i64::MIN`, `i64::MAX`, zero, Boolean values, empty/non-empty strings, Unit, and representative handles/containers. | Optional codegen contains no sentinel comparison, implicit null ABI, payload-byte presence rule, or read of an uninitialized empty payload. |
| `REQ-OA-005` | Ordinary arithmetic, comparison, Boolean, call, index, member, and parenthesized expressions continue to parse. | `a | b`, `a | b | 42`, `true | false`, `0 | 1`, `a ?? b`, and chained/mixed `??` fail before Sema. |
| `REQ-OA-006` | `? | T`, guard else, and leading `?| operation | fallback` parse/typecheck; current resource/task fallback executions retain behavior. | A settlement separator cannot escape its production; general expressions cannot recover meaning through effect/type inference. |
| `REQ-OA-007` | Canonical Optional nodes/types/IR have focused constructor, visitor, verifier, lowering, and ownership tests. | Static search finds no retired value fallback token/helper/node/visitor/codegen path or positive construction test. |
| `REQ-OA-008` | Compiler tests, syntax mirrors, active docs, runbooks, generated indexes, and Better Plan validation agree. | No executable compatibility route, orphan reservation, or stale D02-pending wording remains. |

## Required source cases

### Accepted

```styio
present : ? | i64 = 42
missing : ? | i64 = (?)
also_missing : ? | i64 = [?]
shape_missing : ? | i64 = {?}
minimum : ? | i64 = -9223372036854775808
done : ? | unit = ()
empty_done : ? | unit = (?)

?(condition) => { <| 1 } | { <| 2 }
value = ?| operation | recovery
```

The suite also covers `? | (? | i64)` canonical identity, Optional values in
parameters/results/fields/collections, and existing task/resource settlement
forms from `ACTIVE-SYNTAX.md`.

### Rejected

```styio
value = a | b
value = a | b | 42
value = true | false
value = 0 | 1
value = a ?? b
value = a ?? b ?? 42
value : i64 = (?)
```

The first six fail at the expression syntax boundary. The last parses the empty
literal but fails because ordinary `i64` excludes absence.

## Structural inspections

1. `rg -n "parse_fallback_expr|FallbackAST|SGFallback|TOK_DBQUESTION|DBQUESTION" src tests` returns no match.
2. `rg -n "UndefinedLitAST|SGUndef" src tests` returns no source-value absence path.
3. Token/operator tables contain no mapping from source `"|"` to
   `StyioOpType::Bitwise_OR`; `TOK_PIPE` and `AWAIT_PIPE` remain.
4. Optional IR constructors always carry a canonical Optional `TypeId`; present
   carries one payload and empty carries none.
5. Optional LLVM lowering contains an explicit presence discriminant and no
   comparison with `styio_undef_i64()` or `i64::MIN`.
6. Every remaining `Fallback` test/symbol is classified as current
   effect/resource settlement, diagnostic infrastructure, or unrelated host
   fallback—not the removed value operator.
7. Numeric-only `styio_undef_i64()` consumers are listed for D08 and cannot flow
   into Optional presence decisions.

## Targeted commands

Exact new test names are fixed in the validation-matrix checkpoint. Final
acceptance runs the repository's tokenizer, parser, type-table, Sema, lowering,
IR verifier, codegen, resource-effect, security, and end-to-end language tests,
then:

```text
python scripts/syntax-convergence-gate.py
python scripts/docs-index.py --check
python scripts/docs-audit.py
python scripts/docs-lifecycle.py validate
python scripts/local-info-leak-gate.py --mode worktree
python scripts/manifest_tool.py validate docs/plan
```

## Final evidence record

The final-validation node records the head commit, commands/results, stable
negative diagnostics, deleted symbol/test inventory, anchored-pipe regression
inventory, `i64::MIN` present-value output, TypeId normalization proof, and the
remaining D08-owned numeric sentinel list. Acceptance cannot be weakened by
retaining a value fallback compatibility path or by deleting effect settlement.
