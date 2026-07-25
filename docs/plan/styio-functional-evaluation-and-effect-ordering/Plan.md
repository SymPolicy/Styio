# Styio Functional Evaluation and Effect Ordering Delivery Plan

**Purpose:** Define the bounded product outcome, dependency graph, migration boundaries, and single converged implementation lifecycle for Q03-F.

**Last updated:** 2026-07-20

**Status:** Pending implementation. Q03-F is approved, the plan lifecycle is pending, and all checkpoints remain pending.

**Better Plan ID:** `ca5e328f-b725-4af7-b857-7c979bb3a393`

## 前置条件

The normative source is [Styio Functional Evaluation and Effect Ordering](../../design/Styio-Functional-Evaluation-and-Effect-Ordering.md). This delivery package translates that accepted contract into repository work; it cannot redefine language semantics from current traversal or backend behavior.

Q01-A continues to own exactly:

```text
OperationSummary = { success_type, completion_set }
```

Q03-F owns a separate `EvaluationFacts` domain. Effects, region accesses, totality, normal return, order sensitivity, and optimizer rights never become additional `OperationSummary` fields and never create a second completion-set implementation.

The Q01 directional-flow lifecycle also solely owns canonical `->` parser/AST/Sema/IR shape, settlement syntax/matching and local direct control edges, and deletion of `FlowBind`/task-target/arrow-redirect alternatives. Q03-F waits for and consumes that interface; it owns the node's once-only source/endpoint ENF, two independent prerequisite edges, conflict checking, and surrounding lazy/evaluation CFG only. No compatibility adapter is permitted.

Conversely, this Q03-F lifecycle is the sole implementation owner of the cross-producer Outcome ABI and deletion of global/TLS error state, codegen guards, JIT error symbols, and CLI/native-wrapper polling. Q01 provides family/summary/settlement facts and consumes the Q03-F outcome convention.

## Product outcome

Users receive eager ordinary values without an implicit operand timeline. A strict parent waits for all required values, independent safe-pure siblings may be scheduled in any dependency-respecting order, and unordered order-sensitive siblings receive a source-located diagnostic. Lexical Block items, data/control/resource dependencies, completion stop, cleanup, commit, and publication supply the observable order.

The generated program preserves the same contract directly:

- each logical input is evaluated once and named once before use;
- lazy branches and settlement recovery are real CFG regions;
- completions travel through explicit typed outcomes and successors;
- resource versions and accepted happens-before edges are visible to verification;
- optimizations consume named rights instead of guessing from node kind;
- no ambient error state, hidden left-to-right traversal authority, or compatibility lowering remains.

## Dependency tree

- **Semantic parents:** Q03-F owner; Q01 operation/completion algebra and canonical direction/settlement meaning; Q02 concrete call instances; Q05 operation rows and numerical semantics; accepted Block exit/publication graph.
- **Current lifecycle:** `EvaluationFacts`, Sema summaries, exact-once ENF, typed operation DAG/IR/CFG, Q01 directional-node ordering integration, lazy CFG, explicit outcome transport, generated/native operation-producer migrations, ambient-state deletion, optimization rights, and structural closeout.
- **Consumers:** lowering, verifier, optimizer, LLVM attributes/passes, JIT, CLI/native wrapper, IDE/cache facts, resource topology, tests, and active documentation.
- **Deferred owners:** Q04 ownership, Q05 remaining operations, Q07 patterns, Q08 collections, Q09 resource-family scheduling/fairness, and later concurrency policy.

## Execution shape

The first four checkpoints freeze requirements, repository evidence, validation, and architecture. Implementation then proceeds in this order:

1. establish the unique fact domain;
2. compute canonical Sema summaries;
3. normalize strict inputs into exact-once ENF;
4. build and verify the typed DAG/IR/CFG;
5. fan out Q01 directional-node ordering integration, lazy CFG cutover, and the bounded Outcome ABI;
6. migrate three disjoint producer domains behind the Outcome ABI;
7. converge directional integration, lazy CFG, and outcome work and delete ambient global error state;
8. activate the four optimizer rights only after explicit control and outcome verification exist;
9. remove all old code/tests/gates and record one-revision acceptance evidence.

The machine graph is [Checkpoints.json](./Checkpoints.json). A topological node is a commit-sized responsibility, not permission to ship a mixed semantic state.

## Artifacts

- [Requirements.md](./Requirements.md) — observable requirements, constraints, non-goals, and acceptance target.
- [Evidence.md](./Evidence.md) — accepted authority, current repository paths, external compiler practice, and migration gaps.
- [Validation.md](./Validation.md) — requirement-to-test/static-evidence mapping fixed before implementation.
- [Architecture.md](./Architecture.md) — facts, algorithms, interfaces, data structures, dependency direction, ABI, deletion, and concurrency ownership.
- [Checkpoints.json](./Checkpoints.json) — machine-validated all-pending execution graph.

## Coordination and ownership

The fact, DAG, and ABI modules may be scaffolded in parallel once their preceding design node is accepted. Operation-producer domains may then proceed in parallel with disjoint ownership. The following Q03-F hot paths require a single owner during their respective cutovers: `TypeInfer`, ENF/AST-to-SGIR lowering, SGIR verifier/walker, codegen, existing error state, JIT registration, and the CLI/native wrapper. Q01 retains parser/AST ownership for canonical direction/settlement. Concurrent changes in shared files must be incorporated, never reverted.

The Q01 canonical direction/settlement interface is a hard cross-lifecycle prerequisite for the Q03 directional integration checkpoint. If it has not landed when Q03 foundations begin, fact/ENF/DAG/ABI foundations may proceed where independent, but the integration checkpoint waits. Q03-F may not create a competing node, parser/AST path, completion algebra, family representation, or compatibility bridge.

## Single-final-state rule

There is one implementation authority at every layer. No checkpoint may retain:

- old and new effect-summary domains;
- AST substitution alongside once-only `ValueRef` inputs;
- a Q03-specific directional/settlement AST or legacy adapter alongside the Q01 canonical node;
- eager and CFG forms for source-defined lazy behavior;
- explicit outcomes alongside global/TLS error polling;
- one `pure` bit or node-kind heuristic alongside the four proof-scoped rights;
- positive tests, fixtures, docs, visitors, or gates for removed behavior.

Feature flags, compatibility modes, ambient-outcome sentinel/fabricated-value repair, string family dispatch, and temporary release states are outside this plan.

## 验收条件

1. `Requirements.md`, `Evidence.md`, `Validation.md`, and `Architecture.md` remain mutually consistent with the accepted Q03-F owner and Q01/Q02/Q05 boundaries.
2. `Checkpoints.json` validates as an all-pending topological graph with the four design roles before implementation and one terminal structural-zero validation node.
3. Each implementation surface named by the user has a distinct checkpoint: fact domain, Sema, canonical AST/ENF, typed DAG/IR, `->`, lazy CFG, Outcome ABI, three producer domains, ambient-error deletion, four optimizer rights, and final structural cleanup.
4. The final node records one source revision, per-requirement receipts, exact-once counters, graph/CFG/verifier evidence, producer-domain outcomes, structural searches, optimizer negatives, deterministic/randomized schedules, performance measurements, removed fixtures/gates, and standing repository checks.
