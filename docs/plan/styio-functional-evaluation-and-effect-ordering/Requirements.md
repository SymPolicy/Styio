# Styio Functional Evaluation and Effect Ordering Requirements

**Purpose:** Freeze the observable compiler/generated-code requirements and non-overreach boundaries for implementing accepted Q03-F.

**Last updated:** 2026-07-20

## Problem statement

Styio has approved eager values with dependency-only ordering for independent safe-pure siblings and explicit ordering for effects, completions, resources, and Block items. The current implementation does not have one fact domain or executable dependency graph. Parser categories, C++ traversal, AST cloning, eager select lowering, resource-topology validation, and existing ambient error state can each impose a different order.

The product must converge on one static proof model. A successful migration makes author-visible behavior depend only on accepted data, control, completion, Block, resource, ownership, commit, and concurrency edges. It must not preserve a second implementation through a flag, fallback, old visitor, ambient-state poll, or positive compatibility test.

## Users and observable outcomes

- **Language authors** can reason that ordinary values are strict without assuming a left-to-right operand timeline.
- **Application authors** receive deterministic diagnostics when two order-sensitive sibling computations lack an accepted edge, with guidance to prebind/settle them in consecutive Block items or use an existing task construct.
- **Compiler and generated-code maintainers** have one immutable summary contract, one operation graph, one exact-once lowering discipline, and one explicit completion transport.
- **Tooling authors** read the same Sema facts and stable diagnostics instead of reconstructing effects or order from syntax.
- **Optimizer authors** request a named transformation right and receive fail-closed answers from verified facts.

## Functional requirements

### `REQ-FEO-001` — Strict values and exact-once logical evaluation

Ordinary application, operator, composite, and index/selector inputs are eager prerequisites. Each source expression at a use site denotes exactly one logical evaluation. The parent runs only after every required child returns a normal value. Inlining or resource-method expansion binds receiver and arguments once; zero, one, or repeated parameter references cannot suppress or repeat an order-sensitive argument evaluation. Q03-F introduces no implicit thunk or call-by-need update cell.

### `REQ-FEO-002` — Dependency-only safe-pure siblings

Independent siblings proven effect-free, completion-free, total/normal-returning, resource/identity-unobservable, and semantics-preserving for the requested transform have no author-visible source-order timeline. Any stable topological choice is an implementation detail. Absence of an order edge does not create a task, thread, parallelism, fairness, or race guarantee.

### `REQ-FEO-003` — Fail-closed order-sensitive conflict detection

An ordinary parent with two order-sensitive children lacking a data, control, resource, explicit happens-before, ownership, commit, or separately accepted concurrency edge is ill-formed. Sema identifies both source ranges and the missing-order category. Unknown/native facts, non-empty completion sets, unknown normal return, `never`, volatile/resource effects, and unproven alias/version relations are conservative order-sensitive facts. The compiler may not choose source, pointer, hash, traversal, backend, or completion-observation order.

### `REQ-FEO-004` — Block sequence, completion stop, cleanup, and publication

Top-level order-sensitive items in one lexical Block establish the accepted sequence. If an item completes without a normal value, later ordinary Block items do not start and the current candidate is not published. Already registered mandatory exit obligations still follow the accepted exit graph and prior external effects are not rolled back. Safe-pure work may move only when the exact transformation right proves no observable change. Candidate readiness and publication remain separate.

### `REQ-FEO-005` — Separate immutable summary domains

Q01/Q02 `OperationSummary` remains exactly `{ success_type, completion_set }`. Q03-F adds one independent immutable `EvaluationFacts` record per typed computation, containing or referencing canonical effect/access facts, normal-return/termination facts, and derived optimization rights. The implementation must not fork completion-set storage or mutate `OperationSummary` to encode temporal order. Unknown facts are an explicit conservative top value.

### `REQ-FEO-006` — Canonical Sema inference and publication

Sema derives `EvaluationFacts` once per typed node from literals, built-in descriptors, endpoint/resource contracts, callable summaries, and native/FFI declarations. Calls use Q02 concrete instances; recursive call components use a deterministic bounded fixed point over a finite lattice. Unknown or unmodeled native behavior fails closed. Diagnostics, IDE facts, incremental cache fingerprints, IR lowering, and optimizers consume the same canonical record; no layer re-infers effects from AST kind or backend instructions.

### `REQ-FEO-007` — Canonical AST and Evaluation Normal Form

Frontend representation separates syntax from evaluation facts. Before DAG/IR construction, ENF assigns a unique `ValueRef`/definition to each callee, receiver, argument, operand, composite member, source, endpoint, condition, scrutinee, index, and selector that is evaluated. Child lowering cannot rely on C++ function-argument evaluation order. AST substitution or cloning for parameter references is removed. Source spans and semantic identities remain traceable through normalization.

### `REQ-FEO-008` — Typed operation DAG and deterministic scheduler

The compiler builds a typed DAG with explicit edge kinds for data, control/lazy, completion-stop, ordered effect, explicit happens-before, ownership/borrow/drop, consume, commit, and backpressure. Resource topology supplies capability, alias, region, and accepted HB facts; it is not a second scheduler. Per-region version/last-writer/reader-epoch structures derive RAW/WAR/WAW edges without pairwise graph scans. A stable Kahn-style scheduler is deterministic for reproducible builds and reports cycles/conflicts, but tie-breaking does not become language semantics. Construction and scheduling are `O(V + E)` apart from documented alias queries.

### `REQ-FEO-009` — Typed IR/CFG and verifier conservation

Executable IR operations carry stable operation/value identities, `EvaluationFacts`, typed normal values, and explicit normal/completion/control successors. Lazy regions, merge values, resource versions/tokens, cleanup, commit, and publication joins are structural CFG facts. The verifier checks single definition, dominance, summary conservation, completion successor completeness, lazy non-execution, exact-once inputs, accepted edge preservation, resource-version use, and cleanup/publication convergence. Backend repair of unresolved types, completions, order, or missing returns is prohibited.

### `REQ-FEO-010` — Q01 directional-node ordering integration

Q03-F consumes the Q01-owned canonical directional node and its typed Unit/endpoint/completion facts without rebuilding parser, AST, Sema shape, settlement matching, or local direct control edges. Source value and endpoint capability become two independent strict prerequisites, each evaluated exactly once; transfer starts only after both return normally. If both preparations are unordered order-sensitive computations, Sema rejects the direct expression. Q01 exclusively owns deletion of parser target-shape forks, task-target/await/pull state, `FlowBindAST`, `SIOFlowBind`, and arrow-owned `ResourceRedirectAST`. If that canonical interface is unavailable, the Q03 integration waits and creates no compatibility adapter.

### `REQ-FEO-011` — Lazy/selecting constructs use explicit CFG

Short-circuit booleans, conditionals, matches, guards, fallbacks, and `?|` evaluate the decision/scrutinee/operation once and execute only the selected continuation. Q03-F consumes Q01-resolved settlement arms/families and owns their surrounding lazy execution/order CFG, not a second settlement AST or matching algebra. Settlement success bypasses recovery; one completion selects at most one named arm or safe fallback; unhandled completion propagates. Merge values use typed phi/block arguments after branches. `select` is legal only after both inputs already exist and their evaluation is semantically unconditional and safe; it cannot implement a source-defined lazy construct. Eager `SGFallback`, `SGWaveMerge`, `SGGuardSelect`, or equivalent source-reachable behavior is removed or redefined as verified CFG.

### `REQ-FEO-012` — Bounded explicit Outcome ABI

This Q03-F lifecycle is the sole implementation owner of compiler/generated/native outcome transport. Compiler/generated-code boundaries carry normal success and finite nominal completions explicitly. Each concrete operation has a compile-time `OutcomeLayout`: a bounded success/completion discriminator, stable family ordinal mapping, typed success/payload out locations or direct IR results, and cleanup ownership. This is an internal calling convention, not a source `Result` type or heap exception. It uses no string family lookup, dynamic registry, hidden allocation, global/TLS error state, unwinder, handler stack, or unbounded payload storage. Direct generated calls may use CFG/multiple results; native/FFI adapters use an equivalent explicit status/out convention. Q01 supplies family/summary/settlement facts and consumes this ABI.

### `REQ-FEO-013` — Numeric, parse, container, and matrix producers

Arithmetic checks, numeric/text parsing, collection operations, dictionary/list helpers, and matrix helpers return their Q01/Q05-declared outcomes through the bounded ABI. Success values and completion families are unambiguous for zero/false/empty results. Constant evaluation and generated execution share the same summary and completion edge. Checked overflow, parse failure, bounds/shape/resource failure, and strict numerical behavior cannot fall through a sentinel or ambient error.

### `REQ-FEO-014` — File, resource, cleanup, and backpressure producers

File/resource acquire/read/write/flush/close, snapshot/commit, cleanup/drop, and admitted backpressure helpers return explicit outcomes and contribute access/ordering facts. Failure cannot be recovered by polling shared state. Pending writes, barriers, cleanup ownership, and publication remain visible to DAG/CFG verification. Every producer either returns its declared outcome or is rejected as Unknown; it cannot silently log and continue.

### `REQ-FEO-015` — Task, call, native, and FFI producers

Task create/run/join/settle, ordinary generated calls, native helpers, and FFI adapters publish explicit normal/completion outcomes. Task scheduling/fairness/cancellation remain externally owned, but accepted join/settlement edges are represented. Unknown native code has conservative effects and normal-return facts. Adapters cannot translate failures by setting ambient state, returning fabricated `i64`/`Undefined`, or depending on a process-wide string subcode.

### `REQ-FEO-016` — Delete ambient global error authority

As the sole owner of this deletion, Q03-F removes the existing ambient error implementation after every producer domain and generated consumer uses explicit outcomes. Delete all transitive consumers: global/TLS flags and strings, snapshots, set/clear/has/last/match APIs, string-family matching, codegen guard helpers, resource-effect depth/guard state, JIT symbols, native-wrapper polling, CLI clear/poll/report logic, and compatibility tests. Logging may remain only as logging; it cannot carry semantic completion state.

### `REQ-FEO-017` — Four independent optimization rights

The compiler derives and verifies four distinct rights:

1. `reorder-exact-once` preserves one logical evaluation, value, completion, normal return, effects/accesses, resource version, and publication observations.
2. `speculate` additionally proves totality, completion freedom, effect freedom, and identity/resource unobservability before evaluating before demand.
3. `duplicate` additionally proves stable value/numerical/identity behavior and no observable evaluation-count, allocation, lifetime, or cost consequence.
4. `elide` proves the strict computation can be omitted because the exact parent contract does not need its result and totality/completion/effect/identity/resource observations are absent.

No single `pure` bit, `EffectKind` enum, AST class, or LLVM heuristic implies all four rights. CSE, rematerialization, inlining, constant folding, DCE, fusion, reassociation, and resource-write coalescing request the rights they actually exercise.

### `REQ-FEO-018` — Backend and constant-evaluation proof preservation

Constant evaluation and generated execution consume the same operation descriptor, dependency edges, and rights. LLVM memory/effect, `willreturn`, `noreturn`, `speculatable`, read/write, no-free, no-sync, fast-math, reassociation, or similar attributes are emitted only from facts whose proof meets the backend contract. Strict IEEE floating behavior, signed zero, checked overflow, completion exits, volatile/native effects, allocation/identity, lazy non-execution, cleanup, commit, and publication cannot be weakened.

### `REQ-FEO-019` — Structural convergence, validation, and performance

The final source tree has no source-reachable old Q03-F implementation, compatibility flag/layer, obsolete Q03 ENF/DAG/CFG/Outcome visitor or adapter, ambient-error ABI, optimizer heuristic, positive legacy fixture, or test/gate whose purpose is to preserve removed Q03 behavior. Tests cover exact-once counters, all dependency edge classes, randomized legal schedules, conflict/cycle diagnostics, lazy branches, directional-node integration, generated/native producer outcomes, cleanup, JIT/CLI/native execution, strict arithmetic, verifier negatives, fuzz/deep graphs, deterministic output, and large-graph performance. Active documentation and tooling report Q03-F as implemented only after the same-revision evidence passes. Q01 legacy-direction removal is verified as a prerequisite, not re-owned here.

## Constraints

1. Use stable ordinals/IDs and deterministic iteration for diagnostics and serialized facts; raw pointers, hash iteration, thread timing, or session-specific addresses cannot affect output.
2. Keep memory proportional to graph and declared completion/access facts. No dynamic handler registry, per-node heap exception object, pairwise access matrix, or repeated whole-graph scan is accepted.
3. Unknown information is conservative. Optimization and composition must fail closed rather than infer safety from current LLVM output.
4. The three producer domains may migrate in parallel only after the Outcome ABI and typed IR contracts are frozen, with disjoint ownership and one final ambient-error deletion owner.
5. Q01 directional-node integration, lazy CFG, explicit outcomes, and ambient-error deletion are one Q03 product cutover train once the Q01 interface exists; no intermediate release or compatibility toggle is allowed.
6. Removed code and documentation must not remain in tests, CI gates, generated indexes, or runbooks.

## Non-goals and transferred ownership

- Q04: copy/move/borrow/capture, view lifetime, endpoint ownership, partial moves.
- Remaining Q05/Q06: new numeric relations/conversions and text-unit policy.
- Q07: pattern legality, overlap, exhaustiveness, and destructuring ownership.
- Q08: collection index/slice/iterator/demand protocols.
- Q09 and concurrency owners: resource-family extension, scheduling, fairness, cancellation, cross-stream ordering, escalation policy.
- New source syntax, ordering operators, handlers, retries, continuations, automatic parallelism, or rollback.
- A managed runtime, universal result value, heap exception, dynamic family lookup, or transaction log.

## Final acceptance target

Acceptance requires one source revision where every requirement has executable/static evidence, the typed graph and verifier reject invalid states, all explicit-outcome producer domains pass, the optimizer rights have positive/negative proofs, structural searches find no old authority, graph/compile/generated-execution budgets are recorded, and every relevant compiler, test, sanitizer/security, syntax, documentation, lifecycle, local-information, and Better Plan gate passes or records a clearly unrelated pre-existing failure without weakening Q03-F.
