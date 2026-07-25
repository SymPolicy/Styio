# Styio Optional Absence Boundary Plan

**Purpose:** Deliver the frozen `? | T` absence model and remove every ordinary
value-level fallback/coalescing path without disturbing grammar-anchored effect
settlement.

**Last updated:** 2026-07-16

**Status:** Pending implementation. Owner decisions `O01-Q01`, `O01-Q06..Q07`,
and `P01.13-B` / D02 are frozen; the current compiler still contains a bare-pipe
fallback AST/IR path, an orphan `??` token, overlapping empty/undefined nodes,
and sentinel-based absence behavior.

## 前置条件

1. **并行：** Repository evidence, external-language evidence, fixture
   classification, test discovery, and documentation audits may run in
   parallel. Token/parser/AST, TypeTable/Sema, and IR/codegen implementation are
   ordered by their actual interface dependencies.
2. **子智能体：** Sub-agents may perform read-only audits and disjoint test or
   documentation work. One coordinator reconciles the grammar, canonical
   TypeId, AST/IR ownership, native layout, and anchored-settlement boundary.
3. **基座：** Reuse the current parser authority, session-local TypeTable,
   StyioIR verifier/walker, LLVM backend, diagnostic/test harnesses, syntax
   convergence gates, and documentation workflow. Missing cross-plan substrate
   belongs to `styio-common-foundation` before this feature depends on it.
4. The source contract is
   [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md),
   especially `O01-Q01`, `O01-Q06..Q07`, and `P01.13-B` / D02.
5. `? | T`, `(?)`, `[?]`, and `{?}` are accepted design; no `Option[T]`, word
   constructor, unwrap, coalescing, or default operator is added.
6. `?| operation | fallback` is an effect/resource/task settlement production.
   Its fallback branch and tests remain current and are not part of the removed
   value-operator implementation.
7. D08 still owns general numeric failure behavior. This plan removes sentinel
   use from Optional absence and proves that `i64::MIN` is a present Optional
   payload; it does not silently choose division/overflow policy for D08.

## Delivery target

- ordinary `T` has no empty state;
- `? | T` is one canonical Optional type whose empty and present branches are
  represented independently from every legal `T` payload bit pattern;
- `(?)`, `[?]`, and `{?}` lower through one empty-Optional path;
- contextual `T`-to-`? | T` branch injection is explicit in typed IR, not a
  source wrapper or general subtyping rule;
- `? | (? | T)` canonicalizes to `? | T`;
- `a | b`, `a | b | 42`, `true | false`, `0 | 1`, and every `a ?? b` form fail
  in syntax before type-directed interpretation;
- `FallbackAST`, `SGFallback`, `TOK_DBQUESTION`, the value-pipe operator map,
  source-value `SGUndef`, and their positive tests are deleted in one migration;
- `TOK_PIPE` and resource/guard/type uses remain because their grammar owners
  are still active.

## Scope

1. Optional type parsing, canonicalization, expected-type branch injection, and
   empty-literal typing.
2. One Optional-aware AST/StyioIR representation and direct LLVM tagged lowering
   with no managed runtime helper.
3. Deletion of the dormant general value fallback pipeline and orphan `??`
   token/tooling paths.
4. Removal of `i64::MIN` and `SGUndef` as absence encodings; audit and route
   non-absence numeric sentinel uses to D08.
5. Positive Optional cases, negative operator cases, anchored-pipe regression
   cases, structural deletion checks, documentation, and tooling convergence.

## Non-goals

- No Optional pattern/match, predicate, unwrap, map, coalesce, or default syntax.
- No change to the P01.14-A directional-flow/settlement contract: `?|` settles
  an operation, settlement results use ordinary binding, and a generic
  `operation -> destination` remains orthogonal to Optional absence.
- No decision for arithmetic overflow, division-by-zero, NaN, or other D08
  numeric failure policy.
- No implicit C ABI layout, user-defined union framework, general generics
  redesign, or speculative niche optimization.
- No compatibility parser, deprecated AST, warning mode, or retained positive
  test for removed fallback behavior.

## Execution graph

The machine-validated graph is
[Checkpoints.json](./styio-optional-absence-boundary/Checkpoints.json). Plan-local
delivery contracts are:

- [Requirements.md](./styio-optional-absence-boundary/Requirements.md)
- [Evidence.md](./styio-optional-absence-boundary/Evidence.md)
- [Validation.md](./styio-optional-absence-boundary/Validation.md)
- [Architecture.md](./styio-optional-absence-boundary/Architecture.md)

## 验收条件

All `REQ-OA-*` mappings pass on one head commit; Optional values use a real
presence discriminant; `i64::MIN` round-trips as present; no removed token,
value-fallback AST/IR/codegen path, undefined-as-absence node, or positive
compatibility fixture remains; and every current anchored `|` / `?|` role keeps
its positive parser, Sema, and execution evidence.
