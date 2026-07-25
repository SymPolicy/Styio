# Styio Optional Absence Boundary Architecture

**Purpose:** Define the type, AST, IR, native layout, parser boundary, deletion,
and validation architecture for `? | T` with no ordinary fallback operator.

**Last updated:** 2026-07-16

## Architectural invariant

```text
source grammar -> canonical Optional TypeId -> typed empty/present IR
               -> explicit native discriminant -> guarded payload access
```

Absence is never inferred from payload bits. Rejected fallback punctuation never
reaches semantic analysis. Effect settlement follows a separate syntax-selected
path opened by leading `?|`.

## Module map and dependency direction

| Layer/module | Responsibility | May depend on |
|---|---|---|
| `StyioToken` | Atomic `?`, `|`, and maximal-munch `?|`; no `??` compound or bare-pipe value operator map. | Source characters only. |
| `StyioParser` | Parse `? | T`, the three exact empty atoms, legal guard/settlement separators, and reject general value pipe. | Tokens and AST factories. |
| `StyioAST` Optional nodes | Represent Optional type syntax and one empty atom; no fallback expression node. | Canonical source spans and child ownership. |
| `StyioSession/TypeTable` | Intern structural Optional types and normalize Optional-of-Optional. | Symbol interner and earlier payload `TypeId`. |
| `StyioSema` | Expected-type present injection, empty typing, joins, assignment/argument/result checks, and anchored settlement branch compatibility. | AST plus canonical TypeIds. |
| `StyioIR` Optional nodes/verifier | Carry typed `None`/`Some` facts and make invalid payload access unrepresentable or verifier-rejected. | Canonical TypeIds and typed payload IR. |
| `StyioLowering` | Convert typed Optional AST/Sema facts once into Optional IR; no sentinel or fallback reconstruction. | Sema proof and IR factories. |
| `StyioCodeGen` Optional lowering | Cache native layout by Optional TypeId, emit tag/payload construction and guarded projection/drop. | Verified Optional IR and LLVM types. |
| Tooling/tests/docs | Mirror syntax and prove deletion plus anchored-role preservation. | Source contract and public diagnostics. |

Dependencies flow downward only. Codegen cannot infer Optionality from LLVM value
shape, Sema cannot reinterpret generic punctuation, and resource settlement
cannot feed a detachable `|` operator back into the expression parser.

Optional-specific AST, IR, lowering, and codegen logic should live in focused
files/modules rather than extending already mixed fallback sections. Existing
visitor interfaces may dispatch to those modules; no new registry or managed
runtime abstraction is justified.

## Chosen patterns

### Structural type interning

Extend the canonical type key with a type form and payload `TypeId`. Conceptually:

```text
TypeKey { form = Optional, payload = TypeId(T) }
```

`optional(payload)` first resolves the payload; if it is already Optional, it
returns that canonical TypeId. Otherwise it interns one key. This hash-consing
earns its complexity because equality, substitution, and normalization become
one O(1) identity test. Encoding `"? | T"` in a name string would duplicate
parsing and make nested normalization unreliable.

Compiler “unknown/not inferred yet” continues to use the invalid/undefined type
state and is never a value branch. Empty Optional is a typed value, not
`StyioDataTypeOption::Undefined`.

### Discriminated native value

The initial layout is deliberately explicit:

| Type | Native semantic fields |
|---|---|
| `? | unit` | `i1 present` |
| `? | T` for other `T` | `{ i1 present, Payload(T) }` |

LLVM owns target padding/alignment. The layout is internal and is not promised
as a C ABI. Inactive payload storage may be deterministically zero-initialized to
avoid poison/uninitialized-byte propagation, but those bits are not a `T` value:
the verifier and generated CFG prohibit projection, drop, comparison, or use
unless `present == true`.

This pattern is necessary because `T` retains every legal bit pattern. A later
niche optimization is a separate proof-carrying optimization keyed by TypeId;
it is not part of initial delivery and can never choose `i64::MIN` for `i64`.

### Syntax-selected roles

The parser uses production state, not type queries:

```ebnf
optional_type       = '?' PIPE type ;
optional_empty      = '(' '?' ')' | '[' '?' ']' | '{' '?' '}' ;
general_expression = /* no PIPE and no DBQUESTION production */ ;
guard_else          = guard_head then_branch PIPE else_branch ;
settlement          = AWAIT_PIPE operation { PIPE recovery_or_handler } ;
```

`TOK_PIPE` remains atomic punctuation used by the three anchored productions.
`AWAIT_PIPE` remains a maximal-munch token. `DBQUESTION` is deleted; adjacency of
two `?` characters does not create a reserved feature.

## Frontend and AST

1. `? | T` produces structural Optional type syntax, then interns
   `Optional(TypeId(T))`.
2. `(?)`, `[?]`, and `{?}` all create one `OptionalEmptyAST`-equivalent node with
   the source delimiter retained only for diagnostics/formatting if needed.
3. An ordinary `T` expression is not wrapped in source AST merely because its
   expected type is `? | T`. Sema records a typed present conversion that
   lowering makes explicit once.
4. Empty typing obtains the payload from the expected Optional type or a
   compatible branch join. Without either, inference fails closed; the compiler
   does not invent `i64`, Unit, or an “any Optional”.
5. `NoneAST`, `UndefinedLitAST`, and `FallbackAST` do not survive as compatibility
   nodes. Source `@` remains resource syntax.

## Semantic analysis

Sema implements three small operations over canonical TypeIds:

- `make_optional(T)`: normalize and intern;
- `inject_present(value: T, expected: ? | T)`: validate exact/accepted payload
  compatibility and record one present conversion;
- `join_empty_with(T)`: produce `? | T` only when source control flow explicitly
  supplies the empty branch.

Assignment, parameter, result, field, and collection-element checking reuse
these operations. There is no general subtyping relationship and no truthiness,
sentinel, purity, or effect fallback. Repeated normalization is not scattered
through visitors.

Settlement remains separate: after leading `?|`, its existing AST owns an
operation plus recovery/handler branches. Sema checks recovery compatibility
against the operation's result type. It never creates a value-fallback AST.

## StyioIR and lowering

Use two typed constructors (names may follow repository naming conventions):

- `OptionalNone(optional_type_id)` with no payload operand;
- `OptionalSome(optional_type_id, payload)` with exactly one verified payload.

The verifier checks that the TypeId is Optional, `Some` payload matches the
canonical payload TypeId, and no generic operation projects an absent payload.
`SGUndef` and `SGFallback` are deleted. Lowering consumes Sema's typed facts and
does not retest values or inspect source punctuation.

For memory-backed owning values, construct/copy/drop follows tag discipline:
construct payload before publishing `present = true`; copy/drop payload only for
present; None owns nothing. This reuses the existing ownership policy and does
not add fallback-specific cleanup behavior.

## Native code generation

Codegen caches one LLVM layout descriptor per Optional TypeId to avoid rebuilding
types and field indices. Empty emits `present = false`; present evaluates its
payload exactly once and emits `present = true` with that payload. Every payload
read is dominated by a presence check established by verified IR/control flow.

There is no runtime helper, heap box, reflection record, universal validity
function, or `i64::MIN` comparison. `? | unit` specializes to its tag because
Unit has zero payload bytes while presence remains observable.

## Deletion boundary

Delete as one connected implementation:

1. `DBQUESTION` tokenizer/token-string/IDE/test routes.
2. `parse_fallback_expr` and all calls, replacing them with the correct existing
   non-pipe expression entry.
3. `FallbackAST`, its node kind, declarations, visitors, clones, repr, topology
   traversal, Sema, lowering, ownership, and positive tests.
4. `SGFallback`, `SGUndef`, their verifier/walker/optimizer/type/codegen/repr
   paths, and positive tests.
5. The source `"|" -> Bitwise_OR` operator mapping/precedence path that could
   revive general `a | b`; do not remove `TOK_PIPE`.
6. Sentinel-as-absence comparisons and old `UndefinedLitAST`/`NoneAST` routes.

Preserve resource/effect AST fallback members, diagnostic codes, parser paths,
and execution tests under leading `?|`. Preserve guard else and type union.
Remaining numeric sentinel consumers are recorded for D08 and receive no new
semantics in this plan.

## Diagnostics and migration

- Bare `|` in a general expression reports one syntax-owned diagnostic at the
  pipe; it does not mention operand types or suggest another fallback operator.
- `??` reports removed/unsupported punctuation without reserving a token or
  compatibility AST.
- `(?)` under ordinary `T` reports the static type mismatch.
- Missing payload context for an empty literal reports that an Optional payload
  type is required.

Old executable positive fixtures are removed. Only the current negative inputs
remain, expanded to cover chained, Boolean, integer, and `??` hazards.

## Complexity and memory

- Parsing remains O(source length); no speculative parse/type feedback loop.
- Optional type construction is amortized O(1) through the existing hash table;
  equality is O(1) TypeId comparison.
- Each non-Unit Optional adds one logical tag plus target alignment; Unit adds
  only the tag. No heap allocation is required solely for Optionality.
- Codegen layout lookup is O(1) cached by TypeId.
- Deleting fallback visitors and eager alternate evaluation reduces AST/IR size,
  code paths, and generated work.
