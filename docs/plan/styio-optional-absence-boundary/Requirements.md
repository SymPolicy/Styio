# Styio Optional Absence Boundary Requirements

**Purpose:** Define the source and compiler requirements for explicit Optional
absence and the exclusion of ordinary value fallback/coalescing.

**Last updated:** 2026-07-16

## Decision trace

`O01-Q01` freezes that ordinary `T` never contains absence. `O01-Q06` freezes
`? | T` plus `(?)`, `[?]`, and `{?}`. `O01-Q07` freezes
`? | (? | T) == ? | T`. `P01.13-B` / D02 freezes that Styio has no ordinary
value fallback/coalescing operator and releases `??`.

## Users and outcomes

1. Authors can see possible absence in every field, parameter, result, binding,
   and intermediate type.
2. Compiler and library authors never reserve a legal payload as an empty marker.
3. Tooling assigns each `|` from grammar before Sema and never guesses from types.
4. Effect/resource recovery remains visibly anchored by leading `?|`.

## Functional requirements

### REQ-OA-001 — Static absence floor

An ordinary `T` has exactly the value domain of `T` and cannot contain or receive
the empty state. Every possibly absent value is statically `? | T`. Zero,
`false`, empty text, NaN, null-like handle bits, `unit`, `never`, compiler
unknown type, and failure are not Optional empty.

### REQ-OA-002 — Canonical Optional type and values

`? | T` is the only Optional source type. In an expected `? | T`, an ordinary
`T` expression selects the present branch. `(?)`, `[?]`, and `{?}` select the
same empty branch and lower through one semantic node. An empty literal with no
payload type available from an expected type or a compatible join fails type
inference instead of manufacturing `T`.

### REQ-OA-003 — Set-like normalization

The canonical type layer enforces `? | (? | T) == ? | T`. Type equality,
substitution, branch joins, interning, diagnostics, and IR use the normalized
type identity. Distinct empty meanings require an explicitly tagged type and are
not encoded as repeated Optional layers.

### REQ-OA-004 — Representation preserves all payloads

Optional presence is a semantic discriminant separate from the payload. The
initial lowering uses an explicit tag and may omit only a `unit` payload field.
Every legal `T`, including `i64::MIN`, remains present and round-trips unchanged.
No sentinel, pointer identity, payload byte count, or speculative niche decides
presence. Optional unions acquire no implicit foreign ABI layout.

### REQ-OA-005 — No ordinary fallback/coalescing expression

The general expression grammar rejects `a | b`, `a | b | c`, and `a ?? b`
before Sema. There is no value-fallback domain, lazy/eager rule, result algebra,
precedence, associativity, overload, AST, IR, codegen, formatter, or IDE role.
Effect, purity, truthiness, sentinel checks, or inferred operand types may not
reinterpret rejected syntax.

### REQ-OA-006 — Anchored pipe roles remain distinct

The tokenizer/parser preserve `? | T`, guard else, and the recovery/handler
separator inside leading `?|` settlement. `?| operation | fallback` type-checks
its operation and recovery branch under the effect-settlement contract but does
not expose `|` as a detachable value operator. `TOK_PIPE` and `AWAIT_PIPE`
remain; `TOK_DBQUESTION` does not.

### REQ-OA-007 — One-shot implementation deletion

Delete `parse_fallback_expr`, `FallbackAST`, `StyioNodeType::Fallback`,
`SGFallback`, their declarations/visitors/cloners/verifiers/optimizer/repr/
topology/codegen routes, the `Bitwise_OR` source map for bare `|`,
`TOK_DBQUESTION` and IDE/token tests, source-value `UndefinedLitAST`/`SGUndef`
absence routes, and positive tests that instantiate them. Do not delete resource
settlement fallback members, diagnostics, or execution tests merely because
their names contain “fallback”.

### REQ-OA-008 — Converged diagnostics and evidence

Negative fixtures cover bare and chained `|`, Boolean/integer ambiguity cases,
and `??`. Positive fixtures cover Optional empty/present values, normalization,
containers/functions, all legal payload extremes, and every anchored pipe role.
Parser, Sema, IR, codegen, tooling, active docs, runbooks, and generated indexes
agree on one contract. Removed behavior survives only as current negative syntax
evidence.

## Non-functional constraints

1. Optional type equality is O(1) after interning; normalization occurs once at
   construction rather than through repeated string parsing.
2. The initial native layout is deterministic and requires no managed runtime,
   heap box, reflection, or generic fallback helper.
3. Empty construction stores no uninitialized payload that can later be read.
4. Present construction evaluates and owns its payload exactly once under the
   existing value ownership rules.
5. The change converges to one parser, one AST/IR model, and no compatibility path.

## Scope and non-goals

Owned modules are `StyioToken`, `StyioParser`, Optional/type nodes in `StyioAST`
and `StyioSession`, relevant `StyioSema`, `StyioIR`, `StyioLowering`,
`StyioCodeGen`, `StyioToString`, `StyioResourceTopology`, IDE syntax mirrors,
tests/fixtures, and active documentation. Pattern syntax, convenience APIs,
general union types, D08 numeric failure behavior, the P01.14-A directional-
flow/settlement implementation, and foreign Optional ABI are outside this plan.

## Final acceptance target

On one head commit, every `REQ-OA-*` requirement has executable or static
evidence; the Optional discriminant is independent of payload bits;
`i64::MIN` is present; all removed value-fallback and `??` implementation paths
are absent; and type-union, guard, and effect-settlement pipe tests still pass.
