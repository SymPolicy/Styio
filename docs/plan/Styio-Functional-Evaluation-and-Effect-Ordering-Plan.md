# Styio Functional Evaluation and Effect Ordering Plan

**Purpose:** Deliver accepted Q03-F strict evaluation, dependency-only safe-pure scheduling, explicit effect/completion ordering, exact-once inputs, lazy control flow, and proof-scoped optimizer rights as one compiler/generated-code migration.

**Last updated:** 2026-07-20

**Status:** Pending implementation. Q03-F is design-approved; every checkpoint is pending and no current parser, traversal order, lowering path, runtime error channel, or optimizer heuristic is language authority.

**Better Plan ID:** `ca5e328f-b725-4af7-b857-7c979bb3a393`

## 前置条件

1. **并行：** Read-only inventory and the three generated/native operation-producer domain migrations may proceed in parallel only where file ownership is disjoint. Shared Sema facts, ENF, lowering, codegen, existing error-state, JIT, and driver cutover paths require one convergence owner.
2. **子智能体：** Sub-agents may perform read-only evidence audits or disjoint test/documentation discovery. One coordinator reconciles semantic ownership, shared interfaces, manifests, and final gate evidence; sub-agents do not redefine Q03-F or independently cut over shared compiler paths.
3. **基座：** Reuse Q01 `FamilyId`/`CompletionSet`/`OperationSummary` and canonical direction/settlement interfaces, Q02 concrete call facts, the accepted Block exit graph, existing Sema/SGIR/CFG infrastructure, and the repository Better Plan lifecycle. Do not build a second completion algebra, parser/AST family, or managed runtime foundation.
4. [Styio Functional Evaluation and Effect Ordering](../design/Styio-Functional-Evaluation-and-Effect-Ordering.md) is the sole semantic owner of Q03-F. This plan implements that decision without adding syntax, automatic concurrency, a managed runtime, or a universal source-visible result wrapper.
5. Q01-A remains the sole owner of `OperationSummary = { success_type, completion_set }`. Q03-F introduces a separate immutable `EvaluationFacts`; it must not add effect, termination, control, or optimization fields to `OperationSummary`.
6. The Q01 directional-flow lifecycle is the sole implementation owner of canonical `->` parser/AST/Sema/IR shape, `FamilyId`/`CompletionSet`/`OperationSummary`, settlement matching and local direct control edges, and deletion of task-target/`FlowBind`/arrow-redirect forks. Q03-F consumes that canonical interface and owns only its once-only source/endpoint ENF, two independent prerequisite edges, ordering diagnostics, lazy/evaluation CFG integration, and explicit Outcome transport. If Q01 has not landed, the integration checkpoint waits; it may not create a temporary compatibility node.
7. Q02 owns callable schemes and concrete instantiation. Q05 owns each numeric operation's value/completion relation and strict IEEE/checked-arithmetic policy. Q03-F consumes those summaries and decides when a node may run or transform.
8. This Q03-F lifecycle is the sole implementation owner of the cross-producer Outcome ABI and complete removal of global/TLS error state, codegen guards, JIT symbols, and CLI/native-wrapper polling. Q01 supplies family/summary/settlement facts and consumes this ABI; it does not own a parallel generated-code completion convention.
9. Q04/Q07/Q08/Q09 retain ownership, pattern, collection-protocol, resource-family, scheduling, fairness, cancellation, and backpressure-policy decisions. Q03-F records accepted edges but does not invent those policies.

## Delivery target

The compiler has one static evaluation pipeline:

- Sema attaches one canonical `EvaluationFacts` record to every typed computation while retaining the Q01/Q02 `OperationSummary` unchanged;
- strict callee, receiver, argument, operand, composite member, source, endpoint, index, and selector expressions are materialized once into value definitions before their consumer runs;
- a typed operation DAG records data, lazy/control, completion-stop, ordered-effect, explicit happens-before, ownership/drop, commit, consume, and backpressure dependencies;
- a deterministic scheduler chooses a valid topology only for reproducible compilation, while programs with unordered order-sensitive siblings fail before lowering;
- SGIR/CFG represents normal and completion successors, lazy regions, resource versions, cleanup/publication joins, and one definition per logical evaluation explicitly;
- the Q01 canonical directional operation is consumed without rebuilding it; Q03-F evaluates source and endpoint exactly once, records two independent prerequisites, and starts transfer only after both return normally;
- conditionals, short-circuit forms, matches, guards, and `?|` execute only the selected path and preserve exact-once operation/scrutinee evaluation;
- generated/native operation producers return compiler-bounded explicit outcomes through typed values/out slots and direct branches, never through ambient process/thread error state;
- reorder-exact-once, speculate, duplicate, and elide are independent derived rights, and backend attributes or transformations are emitted only when the corresponding proof exists;
- the Q01 receipt verifies its legacy direction forks are gone before integration; Q03-F deletes its own eager lazy nodes, ambient error APIs/guards, legacy ENF/DAG/CFG/outcome paths, orphan effect heuristics, ambient-outcome sentinel/fabricated-value repair, compatibility fixtures, and gates that protect removed Q03 behavior.

## Scope

1. The Q03-F fact domain: stable computation/region identities, access rows, termination/normal-return facts, conservative Unknown, and four derived optimization rights.
2. Sema summary construction, interprocedural fixed points, built-in/native catalogs, diagnostics, IDE facts, and cache fingerprints.
3. Consumption of Q01 canonical direction/settlement facts plus general Evaluation Normal Form (ENF) with one value definition for each strict input; no Q03 parser/AST replacement.
4. Typed DAG construction, conflict detection, stable topological scheduling, SGIR/CFG representation, verification, and resource-version dependencies.
5. Q03-F ordering integration for the Q01 canonical directional node: once-only source/endpoint ENF, independent prerequisites, transfer/commit edges, and conflict diagnostics, with no legacy adapter.
6. Lazy CFG migration for conditional, short-circuit, match/guard, fallback, wave/select, and settlement paths.
7. One bounded internal Outcome ABI plus three producer migrations: numeric/parse/container/matrix; file/resource/cleanup/backpressure; and task/call/native/FFI.
8. Complete deletion of the existing ambient error state and every frontend/backend/JIT/CLI consumer.
9. Proof-scoped optimization, structural cleanup, targeted/full validation, graph complexity evidence, determinism evidence, and documentation convergence.

## Non-goals

- No new sequencing, ordering, effect, handler, thunk, retry, transaction, or concurrency syntax.
- No implicit left-to-right or right-to-left operand timeline and no automatic task creation from an unordered safe-pure graph.
- No change to Q01 completion-family identity, settlement matching, or `OperationSummary` shape.
- No copy/move/borrow/capture policy, pattern legality, collection indexing policy, resource-family extension, scheduling fairness, or cancellation design.
- No heap exception object, dynamic handler stack, stack unwinder, ambient error channel, resumable continuation, managed thunk/update cell, or universal source `Result` value.
- No compatibility flag, dual frontend/IR/generated-code path, fallback backend repair, or retained positive test for removed behavior.

## Execution graph

The delivery package is under [styio-functional-evaluation-and-effect-ordering](./styio-functional-evaluation-and-effect-ordering/Plan.md):

- [Requirements.md](./styio-functional-evaluation-and-effect-ordering/Requirements.md)
- [Evidence.md](./styio-functional-evaluation-and-effect-ordering/Evidence.md)
- [Validation.md](./styio-functional-evaluation-and-effect-ordering/Validation.md)
- [Architecture.md](./styio-functional-evaluation-and-effect-ordering/Architecture.md)
- [Checkpoints.json](./styio-functional-evaluation-and-effect-ordering/Checkpoints.json)

Requirements, evidence, validation, and architecture precede every implementation node. The fact domain and Sema summaries then establish the single authority; canonical ENF feeds typed DAG/IR; Q01 directional-node integration, lazy CFG, and Outcome ABI work fan out only after those interfaces are verified; the three producer domains converge before ambient global error deletion; optimizer rights activate only after explicit outcomes and verifier coverage exist; final validation proves structural zero.

## One-final-state rule

This migration is not releasable in intermediate compatibility states. Q01 canonical-direction availability is a cross-lifecycle prerequisite; Q03-F adds no legacy-direction adapter. Within this lifecycle, lazy CFG, explicit outcomes, and ambient-error deletion form one cutover train. No feature flag may select old evaluation order, eager select lowering, error globals, or heuristic optimizer behavior. Tests and documentation must move to the new contract or be deleted; removed Q03-F code must not remain protected by a gate.

## 验收条件

1. Every `REQ-FEO-*` label maps to an implementation checkpoint and executable or static evidence on one source revision.
2. `OperationSummary` still contains only success type and completion set; every Q03-F effect, access, normal-return/termination, and optimization fact lives in `EvaluationFacts` or its owned substructures.
3. Side-effect counters prove exactly one logical evaluation of receivers, arguments, sources, endpoints, conditions, scrutinees, operations, and selected recovery expressions, including resource-method parameters referenced zero, one, and multiple times.
4. DAG tests cover data/control/stop/HB/RAW/WAR/WAW/ownership/commit/backpressure edges, conflict diagnostics, cycles, deterministic scheduling, randomized legal schedules, and non-quadratic construction.
5. The Q01 canonical directional operation receives exactly-once source/endpoint values through two independent Q03-F prerequisite edges; lazy constructs execute only selected paths; completion prevents later Block items and early publication while mandatory exit obligations remain ordered.
6. Every generated/native operation-producer domain uses the explicit bounded Outcome ABI. No existing error global/TLS, string-family match, guard helper, JIT symbol, CLI clear/poll/report path, or source-reachable compatibility ABI remains.
7. Reorder-exact-once, speculate, duplicate, and elide each have independent positive and negative proofs. Strict FP, checked overflow, completion, identity/allocation, resource version, volatile/native, cleanup, and publication constraints cannot be bypassed.
8. Targeted tests, verifier negatives, fuzz/stress, full CTest and supported sanitizer/security suites, documentation/lifecycle/local-information gates, performance budgets, structural searches, and Better Plan validation pass with all obsolete Q03 tests and gates removed and the Q01 prerequisite receipt linked.
