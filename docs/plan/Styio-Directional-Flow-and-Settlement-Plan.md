# Styio Directional Flow and Operation Settlement Plan

**Purpose:** Deliver one graphically literal left-to-right data-flow contract for `->` and the accepted static `Q01-A` operation-completion algebra through one generic `?|` settlement migration, deleting every task-target and wildcard-discard interpretation that was never part of the language design.

**Last updated:** 2026-07-20

**Status:** Pending implementation. Direction, settlement composition, and the `Q01-A` operation-completion algebra are frozen. This is their single implementation owner. The accepted `Q03-F` strict-value/dependency-graph/effect-order contract is owned by [Styio Functional Evaluation and Effect Ordering](../design/Styio-Functional-Evaluation-and-Effect-Ordering.md) and its dedicated implementation plan; this Q01 plan consumes that contract but does not implement it. Ownership, endpoint protocols, scheduling, and reverse-flow syntax remain with their registered owners.

## 前置条件

1. **并行：** Repository evidence, test discovery, documentation inventory, endpoint classification, and deletion searches may run in parallel. Parser/AST convergence precedes Sema/IR convergence because every later layer consumes the same directional-operation and settlement-wrapper shapes.
2. **子智能体：** Sub-agents may perform read-only audits and disjoint test/documentation discovery. One coordinator reconciles the grammar, AST, Sema, lowering, diagnostics, and active-SSOT result.
3. **基座：** Reuse the authoritative parser, current generic resource-effect settlement path, ordinary binding model, diagnostics, tests, syntax-convergence gates, and Better Plan workspace. Shared infrastructure belongs to `styio-common-foundation` rather than this plan.
4. The frozen contract is indexed as `D21` in [Styio-Language-Decision-Ledger.md](../design/Styio-Language-Decision-Ledger.md); the review owns only the still-open adjacent questions.
5. This plan does not infer answers for `<-`, chained/intermediate endpoints, ownership transfer, borrowing, backpressure escalation policy, concurrency scheduling, cancellation protocol, endpoint declaration syntax, or completion-family declaration syntax. It also does not duplicate accepted `Q03-F` strictness, dependency, Block sequencing, completion-stop/publication, endpoint-preparation, diagnostic, or optimizer-rights work.

## Delivery target

The compiler and documentation converge on one compositional model:

```styio
left -> destination

answer : T = ?| operation | fallback

answer : T = ?| operation | network(problem) => recover(problem)

?| (operation -> destination) | fallback
```

- `left -> destination` always depicts value/data produced by the left expression or action flowing to the typed destination, location, or receiver endpoint on the right.
- Endpoint families may impose different type, capability, ownership, or protocol checks, but those checks do not create different source meanings for `->`.
- `?| operation | fallback` and exact `family` / `family(binding)` arms settle the whole operation under the accepted `Q01-A` algebra. They neither declare a task target nor change the meaning of an arrow inside that operation.
- `family` and `binding` are grammar metavariables for ordinary identifiers. Example names such as `network`, `problem`, or `io` add no keyword.
- Every operation exposes one success type plus a finite nominal completion set. Successful direction produces Unit; exact recovery is lazy once-only; remaining families propagate statically; catch-all is recoverable-only; no implicit retry or ambient failure runtime exists.
- A caller that wants the settlement result uses the ordinary binding model: `answer = ?| operation | fallback` or its explicitly typed/final variant.
- A directional transfer may itself be the operation being settled. Parentheses make the intended composition explicit until precedence and chaining questions receive their own decisions.
- The dedicated `?| task -> name: T` await-binder route, source-less `?| -> name: T` route, special await-target flags, and their positive compatibility tests are deleted in one migration.

## Scope

1. Directional-flow grammar/AST ownership and typed endpoint interface.
2. One generic settlement wrapper over an operation, recoverable fallback, and exact nominal completion-family arms.
3. Ordinary binding of settlement results without a dedicated target declaration.
4. Removal of await-target lookahead, `CreateAwait`/declare-target flags, bare-freeze handling, task-only parser forks, and downstream compatibility routes.
5. Canonical interned completion-family identities, bounded completion sets, operation/settlement summaries, exact matching and payload scope, safe fallback, normal-result joins, category separation, and static propagation.
6. Removal of ellipsis settlement, discard state, hard-coded family strings, Q01-local implicit result repair/retry, and every positive compatibility fixture. Q01 paths consume the Q03-F bounded Outcome interface; complete global/TLS/JIT/CLI ambient-error deletion remains Q03-F's sole implementation responsibility.
7. Preservation of valid generic `->` operations while all incompatible legacy settlement behavior is replaced in the same migration.
8. Cross-layer fixtures, diagnostics, formatter/editor mirrors, active language documents, runbooks, generated indexes, and deletion checks.

## Non-goals and later owner questions

- Whether `<-` is the exact reverse of `->`, an acquisition operator, or another contract.
- Whether `a -> b -> c` is accepted and, if so, which intermediate value, capability, or acknowledgement continues.
- Accepted `Q03-F` evaluation/effect ordering, including strict prerequisites, dependency-only safe-pure siblings, Block sequencing, endpoint-preparation independence, completion stop/publication, diagnostics, and optimizer rights. Those rules are external constraints owned by the dedicated Q03-F implementation plan, not undecided policy for this migration. Buffering, backpressure escalation, scheduling, cancellation protocol, and concurrency guarantees remain outside this plan. `Q01-A` already forbids implicit retry inside settlement.
- Whether a transfer moves, borrows, copies, streams, redirects, or writes for each endpoint family; those are endpoint/type/protocol questions, not alternative glyph meanings.
- The complete grammar for declaring or constructing typed destination endpoints.
- Implementation of the accepted `Q02-BC` / `Q02-SIG` / `Q02-INF` callable contract and principal constrained inference policy; those facts have their own [Callable Principal Inference plan](./Styio-Callable-Principal-Inference-Plan.md). Completion-family declaration ownership (`Q09`/`Q10`) and work beyond the accepted [operation-completion contract](../design/Styio-Operation-Completion-and-Settlement.md) also remain outside this plan.
- Any compatibility parser, warning-only transition, legacy AST, or second task-specific await syntax.

## Execution graph

The machine-validated graph is [Checkpoints.json](./styio-directional-flow-and-settlement/Checkpoints.json). Plan-local delivery contracts are:

- [Requirements.md](./styio-directional-flow-and-settlement/Requirements.md)
- [Evidence.md](./styio-directional-flow-and-settlement/Evidence.md)
- [Validation.md](./styio-directional-flow-and-settlement/Validation.md)
- [Architecture.md](./styio-directional-flow-and-settlement/Architecture.md)

## 验收条件

1. Every `REQ-DFS-*` and `REQ-OCS-*` requirement maps to executable or static evidence in `Validation.md` and to implementation nodes in `Checkpoints.json`.
2. Parser, AST, Sema, IR, lowering, and codegen expose one directional-flow operation independent of endpoint family and one orthogonal settlement wrapper.
3. `answer : T = ?| operation | fallback` uses the ordinary binding lifecycle; no settlement-specific target declaration or hidden uninitialized slot remains.
4. A generic directional operation can be settled as one operation without reclassifying `->` as a task, resource, export, assignment, or redirect operator.
5. Task-specific await-target, source-less bare-freeze, wildcard discard, hard-coded family matching, Q01-local result repair, and their positive tests are absent; Q01 direction/settlement paths do not consult ambient state, and the Q03-F prerequisite receipt owns/proves complete global/TLS/JIT/CLI deletion. No executable compatibility path remains.
6. Every still-open adjacent question remains explicitly outside the implementation target, and validators do not accidentally freeze it.
7. Exact arbitrary-name family/binder matching, payload scope, safe fallback categories, normal-result joins, propagation, Unit transfer success, and once-only recovery pass through verified IR with bounded compiler-known state and no managed failure runtime.
8. Targeted compiler/tests plus syntax convergence, documentation index/audit/lifecycle, local-information, and Better Plan validation pass on one head commit.
