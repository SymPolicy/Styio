# ADR-0121: Match Bind Canonical StyioIR Optimization

**Purpose:** Record the decision, context, alternatives, and consequences for making equivalent match spellings converge through the standard StyioIR optimizer before LLVM codegen.

**Last updated:** 2026-05-04

## Status

Accepted

## Context

Styio now accepts multiple source spellings for the same integer match workflow:
binding the scrutinee with `#(n = expr) ?=`, matching `expr ?=`
and rebinding the scrutinee in the default arm, and writing guarded integer
patterns such as `(n == 1)`. These forms are intended to express the same
algorithm when the scrutinee is the same value.

If the parser preserves those spellings all the way to LLVM, users pay for a
surface-syntax choice with different control-flow shape, extra declarations, or
temporary naming drift. If the AST lowering layer special-cases individual
source patterns instead, the compiler grows low-level rewrites that only work
for the latest example. Both outcomes make optimizer behavior and exact IR
regression tests less stable than the language semantics require.

## Decision

1. Treat `#(name = expr) ?= { ... }` as a bind-then-match form: the scrutinee is
   evaluated once, assigned to `name`, and the following match uses `name`.
2. Treat `_` and all-underscore identifiers such as `_______` as the same default
   arm spelling in both parser routes.
3. Lower `MatchCasesAST` to ordinary `SGMatch` nodes and keep arm-value
   canonicalization limited to the semantic pattern rule: integer literal arms
   and guarded scrutinee-equality arms map to the same integer arm value.
4. Run `StyioIROptimizer` over `SGBlock`, `SGMainEntry`, and nested child IR
   before LLVM codegen. The optimizer owns sequence-aware canonicalization,
   including MatchCases-compatible rewrites.
5. In the optimizer, hoist only eligible repeated scrutinee rebinds. A default
   arm binding such as `n = expr` may be hoisted before the match only when the
   match scrutinee is structurally the same `expr`, the expression is
   speculatable, the binding is later read as an alias, and surrounding
   statements do not read/write the alias, mutate scrutinee dependencies, or
   cross unknown side-effecting IR before the alias binding.
6. Keep the speculatable expression set explicit and conservative: identifiers,
   dynamic loads, scalar constants, safe arithmetic/comparison/bitwise operators,
   and collection length reads are allowed; calls, I/O, allocation-like nodes,
   division, modulo, and unknown nodes are not speculated.
7. Keep empty lexical scopes from emitting unused runtime helper declarations, so
   source-equivalent programs can compare equal at the LLVM IR string level.

## Alternatives

1. Preserve every source spelling through LLVM and rely on later LLVM
   optimization.
   - Rejected because exact frontend and codegen regressions need deterministic
     canonical IR before external optimizer decisions.
2. Canonicalize only in the parser.
   - Rejected because semantic equivalence depends on the scrutinee and arm
     bodies, which are middle-layer concerns.
3. Hard-code the current `values.length` / `.size` example in AST lowering.
   - Rejected because it would make one benchmark shape look correct while
     leaving equivalent MatchCases over arithmetic or dictionary length outside
     the canonical path.
4. Accept equivalent stdout only.
   - Rejected because the requested contract is identical lowered LLVM IR for
     these accepted forms, not only runtime output equivalence.

## Consequences

Positive:

1. Equivalent match spellings converge through one reusable StyioIR optimizer
   pass and produce the same switch-shaped LLVM IR.
2. Users can write the clearer local spelling without changing backend output.
3. The regression suite now catches runtime algorithm equivalence, exact
   max-element LLVM IR drift, and a generic pure-arithmetic MatchCases
   canonicalization case.

Negative:

1. The optimizer must keep speculation conservative and local; widening the
   whitelist requires tests that prove the expression cannot change observable
   behavior when evaluated before the match.
2. Future pattern kinds must decide whether they belong in this canonical match
   path or should remain distinct lowering forms.
