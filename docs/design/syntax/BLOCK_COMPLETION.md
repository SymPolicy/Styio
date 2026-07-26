# Lexical Block Completion

**Purpose:** Define the canonical value-completion contract for every lexical
Styio Block. This document does not define or activate continuations.

**Last updated:** 2026-07-20

## 1. Canonical forms

```styio
=> expr
=> { expr }
=> { <| expr }

{
    work()
    <| result
}

{ work(); |<| result |; }
```

`<| expr` completes only the immediately owning lexical Block and makes `expr`
that Block's result. `|<| expr |;` is the compressed spelling of the same node
and target; its closing `|;` is mandatory and carries no additional semantic
choice.

Only after the outermost function-body Block has been typed is that Block's
result adapted into the function result. A yield inside a nested Block, closure,
task body, guard branch, match arm, or resource session never crosses that
boundary.

## 2. Single-expression sugar

For a body containing exactly one ordinary expression item and nothing else,
these forms are equivalent:

```text
=> expr == => { expr } == => { <| expr }
```

This is a narrow single-expression rule, not general tail-expression inference.
A multi-item Block does not acquire a value merely because its last item is an
expression. It uses `<|` explicitly when it must produce a non-Unit result.
Whitespace and statement separators never toggle the rule.

## 3. Completion algebra

- Normal fallthrough at a reachable `}` produces `() : unit`.
- `<| expr` contributes the canonical type of `expr` as a normal exit from the
  current Block.
- `<| ()` is legal explicit Unit completion.
- A proven non-completing edge has type `never` and contributes no normal value:
  `join(T, never) = T`.
- Every reachable normal exit must have a compatible canonical result type.
  Reachable `T` and Unit fallthrough are incompatible. The compiler never
  repairs this with zero, a default, absence, or implicit `? | T`.
- A Unit-only consumer rejects a non-Unit yield instead of evaluating and
  discarding its value.
- A sibling or region whose every structural incoming path has already
  completed is a compile-time error independent of optimization.

`unit` is a real one-value type and `never` is an uninhabited type. Neither is a
compiler placeholder. `never` has no value, literal, default, or inference
fallback.

## 4. Publication barrier

Top-level Block items also provide the accepted Q03-F order-sensitive sequence:
an earlier item normally settles before a later order-sensitive item starts,
and its completion prevents later ordinary items from starting. Safe pure work
may move only under the separate as-if rights. This sequence/stop rule is owned
by [Functional Evaluation and Effect Ordering](../Styio-Functional-Evaluation-and-Effect-Ordering.md)
and does not change the result/publication rules below.

Producing a candidate and publishing a result are distinct events. The result
expression is evaluated exactly once into an immutable epilogue-owned candidate.
Ordinary `T` becomes observable only after required logical commit and every
non-transferable lexical exit obligation has reached a terminal outcome. A
settlement failure invalidates the unpublished candidate; recovery must create
an explicit replacement and never revives the failed candidate.

All actual exit obligations are ordered by one verified dependency graph.
Dependencies override stable reverse-registration ordering. Ordering never
depends on pointer identity, hash iteration, wall-clock completion order, or a
scheduler race. A future feature contributes an obligation only after that
feature is separately accepted; this contract does not activate continuations,
detached work, rollback, or another control surface.

Exit actions are internally proven total, typed fallible, or fatal. Bounded
typed failures use compiler-sized fixed storage and deterministic semantic
ordinals. Declaring an action infallible, ignoring a native result, or logging a
failure does not remove its physical failure mode. No managed exception runtime,
growable hidden failure list, truncation, or heap fallback is part of this
contract.

## 5. Explicit non-meanings

- `<|` is not an infix application operator; `f <| a` is removed. Ordinary
  application uses `f(a)` and chained application uses `f(a)(b)`.
- `<|` is not a function-searching `return` and is never non-local.
- `<|` does not define continuation capture, resume, discontinue, ownership, or
  lifetime. Those remain behind their own owner admission decision.
- `|<-` remains reserved. `|>` belongs to the separately defined resource-session
  settlement surface.

## 6. Authority split

- Formal productions: [../Styio-EBNF.md](../Styio-EBNF.md).
- Full language semantics: [../Styio-Language-Design.md](../Styio-Language-Design.md),
  §6.7.
- Token lookup: [../Styio-Symbol-Reference.md](../Styio-Symbol-Reference.md).
- Implementation checkpoints and evidence:
  [Styio Block Completion and Bottom Type](../../plan/styio-block-completion-and-bottom-type/Requirements.md)
  and its Block-exit child plan.
