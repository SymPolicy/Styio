# Styio Functional Evaluation and Effect Ordering Architecture

**Purpose:** Define the single fact model, normalization, graph/CFG algorithms, bounded generated-code outcome convention, optimizer proof interfaces, and one-shot deletion architecture for Q03-F.

**Last updated:** 2026-07-20

## 1. Architectural invariants

1. Q01/Q02 `OperationSummary` remains the sole success/completion fact and has exactly two fields.
2. Q03-F facts are immutable, Sema-owned, separately interned, and consumed by ID; AST, IR, backend, IDE, and optimizer do not each infer their own copy.
3. Every strict source input creates one logical value definition before its consumer. Inlining references values, never argument ASTs.
4. Only graph edges establish observable order. A deterministic scheduler tie-break is reproducibility policy, not language order.
5. Lazy selection and completion stop are explicit CFG. LLVM `select` cannot stand in for a source-defined lazy region.
6. Generated calls and native helpers return bounded explicit outcomes. There is no new language runtime subsystem, managed object, unwinder, dynamic handler registry, or source-visible `Result`.
7. Optimizer rights are separate derived proofs. A `pure` label, AST/IR class, or LLVM attribute is never blanket authorization.
8. Q01 canonical-direction integration, lazy CFG, explicit outcomes, and ambient-error deletion converge in one Q03 release cutover after the Q01 interface is available. Q03 creates no legacy-direction adapter, dual path, or feature flag.

## 2. Fact domains

### 2.1 Operation summary boundary

The Q01/Q02-owned object remains:

```text
OperationSummary {
    success_type: TypeId
    completion_set: CompletionSetId
}
```

`CompletionSet` uses the canonical Q01 representation and stable family identities. Q03-F holds only an `OperationSummaryId`/reference. It does not add effect, ordering, termination, region, or optimizer fields and does not duplicate family-set union/subtraction.

### 2.2 Evaluation facts

The logical Q03-F record is:

```text
EvaluationFacts {
    operation: OperationSummaryId
    accesses: EffectRowId
    return_behavior: ReturnBehavior
    observability: ObservabilityMask
    rights: OptimizationRights   // derived cache; verifier recomputes
}
```

`ReturnBehavior` contains the minimum independent facts required by Q03-F:

```text
NormalReturnFact = AlwaysHasNormalPath | NeverNormal | MaybeNormal | Unknown
TerminationClass = ProvenTerminates | MayDivergeOrTrap | Unknown
```

The finite completion set answers which nominal completion families may occur. It does not prove termination, absence of fatal/trap behavior, or a normal result. `NeverNormal` and `MayDivergeOrTrap` are therefore not encoded as completion families.

`ObservabilityMask` records non-access facts that constrain transforms, including allocation/address identity, lifetime/drop count, resource version, volatile/atomic/native behavior, task/stream consumption, externally observable evaluation count/cost where admitted by the owner, and numerical-semantics constraints. Unknown sets every conservative blocker.

### 2.3 Effect/access rows

An interned effect row is a stable sequence of access descriptors:

```text
EffectAccess {
    region: RegionId
    mode: Read | Write | Consume | Allocate | Release | Commit |
          Close | Synchronize | Backpressure | External | Volatile | Unknown
    stage: AccessStage
    alias_class: AliasClassId
}
```

Stages describe required order inside one operation, not source operand order. Regions may represent storage, a logical resource, console/input, task state, native/unknown state, allocation/identity, or another accepted abstract resource. An Unknown/native access aliases the conservative root region unless a reviewed declaration proves a narrower row.

Use dense compilation-local IDs for graph indexing and canonical stable fingerprints for caches/diagnostics. Pointer values, unordered-map iteration order, thread identity, or process-local addresses never enter serialized facts.

### 2.4 Rights are derived

`OptimizationRights` is a four-bit result of named queries over primitive facts and context. It is cached for speed only. IR construction cannot assert a right directly; the verifier recomputes it from the referenced facts, operand facts, resource versions, parent contract, and requested transform.

The orphan `src/StyioIR/EffectKind.hpp` is not extended into a second domain. Execution either replaces all consumers with the canonical fact query and deletes the enum or, if the filename is reused, replaces its contents/meaning completely with no old enum, comments, or compatibility API.

## 3. Sema summary construction

### 3.1 Per-node publication

After type inference resolves a node's Q01/Q02 `OperationSummary`, Sema computes and interns one `EvaluationFacts` record. A side table keyed by stable semantic node identity publishes `EvaluationFactId`. Syntax nodes remain source representation and do not acquire mutable inference/optimizer fields.

Leaf and built-in facts come from reviewed catalogs:

- literals and immutable value reads are effect-free but receive totality/identity facts appropriate to materialization;
- Q05 operation descriptors supply concrete success/completion and numerical constraints;
- resource/endpoint descriptors supply regions, access modes, capabilities, and accepted barriers;
- task/stream/native/FFI descriptors are conservative unless their declaration supplies a verified contract;
- Block, branch, settlement, cleanup, commit, and publication nodes contribute control/order facts rather than masquerading as ordinary pure values.

Composite facts union child access rows, preserve completion/return behavior, and never erase Unknown. Union and comparison use canonical bit sets/sorted small vectors rather than repeated string scans.

### 3.2 Calls and recursion

Q02 concrete callable instances are the call-summary identity. Build the concrete call graph, compute strongly connected components deterministically, and evaluate a finite monotone lattice to a fixed point:

1. seed declared native/public boundaries from their explicit contracts or conservative Unknown;
2. seed concrete bodies from local nodes and callee references;
3. union effects/completions and weaken return/termination facts monotonically;
4. process SCC members in stable semantic-ID order until no interned fact changes;
5. apply deterministic iteration/size gates and fail closed on a nonconvergent implementation defect.

Completion-set solving remains Q01/Q02-owned. Q03-F consumes its stable set during the same fixed point; it does not narrow a public upper bound from current body accidents.

### 3.3 Conflict analysis

For each ordinary strict parent, Sema/graph preparation partitions children into safe-pure-for-composition and order-sensitive. It computes reachability only through already accepted local data/control/resource edges. Two order-sensitive siblings without a path in either direction produce the required diagnostic before executable lowering. No arbitrary edge is added to make the graph schedulable.

Diagnostics carry both child ranges, fact summaries, and the missing-order category. They recommend consecutive Block prebinding/settlement or an existing explicit task construct; they never recommend a new sequencing operator.

## 4. Q01 canonical interface and Evaluation Normal Form

### 4.1 Canonical frontend prerequisite

Q01 is the sole owner of canonical direction/settlement parser, AST, Sema, and IR shapes, including endpoint capability facts, family identities, arm matching, result joins, local direct control edges, and deletion of task-await/flow-bind/arrow-redirect forks. Q03-F consumes the exact Q01 interface that lands; it defines no replacement `DirectionalFlowAST`, `SettlementAST`, parser branch, visitor hierarchy, or compatibility wrapper.

The Q03 integration needs only stable references to the Q01 node's source expression, endpoint expression, resolved `OperationSummary`, endpoint contract, and (for settlement) resolved local successors. If the Q01 interface is unavailable, ENF/DAG foundations that do not depend on it may proceed, but directional/settlement integration waits.

### 4.2 ENF contract

Normalization returns graph definitions, not a semantically ordered prefix list:

```text
normalize(expr) -> ValueRef
define(OperationId, input ValueRefs..., EvaluationFactId) -> ValueRef
```

For a strict parent, normalize each child exactly once into a unique definition, then add data edges from those values to the parent. The normalization driver may visit children in a stable order for compiler reproducibility, but it cannot emit an observable order edge unless the language graph supplies one.

Callee, receiver, arguments, operands, composite members, base/index, source, endpoint, condition, and scrutinee each obtain a value identity before use. Source ranges and semantic IDs are retained for diagnostics and debugging.

### 4.3 Inlining

Inlining creates a parameter-to-`ValueRef` environment. Every parameter read references the same definition; an unused parameter does not erase its already-required strict argument evaluation, and repeated reads do not re-evaluate it. Resource-method property/call lowering follows the same rule. `StateExprCloneVisitor`, `clone_resource_method_body_latest`, or equivalent AST substitution cannot receive source expressions as replacements after ENF integration.

Child lowering is performed in named statements/graph-builder calls. Code such as `Create(child1->toStyioIR(...), child2->toStyioIR(...))` is prohibited where C++ argument evaluation might become an accidental semantic authority.

## 5. Typed operation DAG

### 5.1 Nodes and edges

```text
EvaluationNode {
    id: OperationId
    output: optional ValueRef
    facts: EvaluationFactId
    source: SourceSpanId
}

EdgeKind = Data | ControlLazy | CompletionStop | OrderedEffect |
           ExplicitHB | OwnershipBorrowDrop | Consume |
           Commit | Backpressure
```

Edges have stable insertion IDs and optional region/version/control labels for verification. Adjacency lists and indegree arrays are indexed by dense `OperationId`.

### 5.2 Edge construction

- Strict inputs add `Data` edges to their parent.
- Decision nodes add `ControlLazy` edges only into selectable regions.
- A Block maintains the last order-sensitive item; its normal successor gates the next order-sensitive item through `OrderedEffect`/`CompletionStop`. Safe-pure work is not chained merely because it appears later in source.
- Resource topology contributes reviewed region, capability, alias, ownership, commit, backpressure, and explicit HB facts through an adapter. It does not select execution order.
- Cleanup/drop and publication nodes consume the accepted Block-exit graph and cannot be synthesized from destructor traversal order.
- Explicit task join/settlement edges are imported; missing concurrency policy remains Unknown, not guessed.

### 5.3 Region-version algorithm

Maintain per canonical region:

```text
RegionState {
    version: RegionVersion
    last_writer_or_barrier: optional OperationId
    readers_in_epoch: small vector<OperationId>
}
```

On read, add RAW from the last writer/barrier and append the reader. On write/consume/commit/close/barrier, add WAW from the last writer and WAR from readers in the current epoch, clear readers, advance the version, and install the new writer/barrier. Read/read needs no edge unless its resource protocol says observation itself is ordered. Unknown aliases map to the conservative common region; proven-disjoint regions stay separate.

Each emitted dependency is an actual graph edge, so total work is proportional to `V + E` plus explicit alias queries. Do not compare every pair of operations or scan every prior graph edge for each access.

### 5.4 Scheduling and cycles

Use Kahn scheduling with:

- dense indegrees;
- a FIFO ready queue seeded in stable `OperationId` order;
- stable adjacency insertion order;
- one visit per node/edge.

This gives deterministic `O(V + E)` scheduling without requiring the lexicographically smallest topology. A test-only fixed-seed alternate ready-node selector validates no-edge safe-pure equivalence. Any cycle reports a bounded stable path and edge kinds. Unordered order-sensitive siblings are diagnosed before scheduling; the scheduler never resolves them by tie-break.

## 6. Typed IR and CFG

### 6.1 IR contract

Executable operations carry `OperationId`, output/input `ValueRef`s, and `EvaluationFactId`. Operations capable of completion expose explicit successors:

```text
normal(success ValueRef) -> normal successor
complete(FamilyId, optional payload ValueRef) -> settlement/propagation successor
```

Lazy constructs own regions/basic blocks and merge with typed phi/block arguments. Resource operations carry input/output region versions or tokens sufficient to verify their DAG dependencies. Cleanup, commit, Block candidate, and publication are distinct nodes/blocks.

### 6.2 Verifier

The verifier rejects:

- missing/duplicate value definitions, use before definition, or failed dominance;
- facts whose `OperationSummary` does not match operand/result types and normal/completion successors;
- a family successor absent from the finite set or a possible family silently dropped;
- eager entry into an unselected lazy region or a merge value without complete predecessor coverage;
- missing completion-stop, Block sequence, ownership/drop, resource-version, commit/backpressure, cleanup, or publication dependency;
- forged optimizer rights or a backend-only repair/default;
- source-defined lazy behavior encoded as both-input evaluation followed by `select`.

Verification runs after construction and after every transforming pass that may change edges, values, facts, or CFG. Optimizers update through graph/IR APIs rather than editing fact IDs or edges ad hoc.

## 7. Q01 directional-node ordering integration

The DAG form is:

```text
source definition ---Data---+
                            +--> transfer --> Unit
endpoint capability --Data--+
```

There is no edge between source and endpoint preparation unless another accepted dependency supplies it. Transfer owns endpoint access/commit/backpressure/completion edges and runs once after both normal values. If both preparations are order-sensitive and unordered, Sema rejects the expression.

Q03 attaches this ENF/DAG/CFG shape only to the Q01 canonical directional node. It does not touch parser/AST canonicalization or own deletion of `FlowBindAST`, `CreateAwait`, await/pull/declare-target flags, `SIOFlowBind`, or arrow-owned `ResourceRedirectAST`; those are Q01 responsibilities and their completed receipt is the integration prerequisite. No Q03 adapter accepts a legacy node. Q03 owns only exact-once source/endpoint definitions, the two independent prerequisite edges, conflict diagnostics, transfer/commit ordering, verifier coverage, and downstream outcome propagation.

## 8. Lazy CFG

### 8.1 Conditional and match

Evaluate a condition or scrutinee once, branch to only the selected region, and merge normal values after branch-specific completion exits. A match pattern test gates its guard; the guard gates its body; a false guard continues to the next admitted arm; a completing guard exits through its completion edge.

### 8.2 Settlement

Evaluate the complete operation once. Q01 supplies resolved family/arm matching, safe-fallback legality, result joins, and local direct successors; Q03 supplies their exact-once scheduling and surrounding lazy CFG/order. The normal successor bypasses recovery. Each resolved completion family reaches only its Q01-selected arm or safe fallback; the selected recovery runs at most once. Completing recovery propagates its own family. Typed normal results merge only from reachable success/recovery predecessors; unhandled completion bypasses the merge.

### 8.3 Existing eager forms

`SGFallback`, `SGWaveMerge`, `SGGuardSelect`, and any equivalent source-reachable forms must either be replaced by region/branch IR or proven to represent already-computed unconditional values with no lazy source meaning. Old visitors, codegen that evaluates both sides before `select`, tests, and docs are deleted in the same node. LLVM `select` remains legal for already-defined values only when selection itself has no lazy/effect/completion obligation.

## 9. Bounded generated-code Outcome ABI

### 9.1 Boundary and representation

This Q03-F lifecycle is the sole implementation owner of the cross-producer Outcome ABI. Q01 supplies `FamilyId`, `CompletionSet`, `OperationSummary`, settlement matching, and local direct control facts and consumes this convention; it does not define a parallel completion ABI. The Outcome ABI is a compiler-owned/generated-code return convention, not a new language runtime system. For each concrete `OperationSummary`, the compiler creates:

```text
OutcomeLayout {
    success_type: TypeId
    families: canonical sorted vector<FamilyId>
    discriminator_width: integer width sufficient for 0..N
    success_location: direct result or caller-owned typed out location
    payload_locations: compiler-sized/aligned branch payload layout
}
```

Discriminator `0` means normal success; `1..N` map at compile time to the layout's canonical family order. Zero-payload families allocate no payload object. Direct generated calls may use an LLVM aggregate, multiple SSA results, or direct branch convention. C/native/FFI boundaries use an explicit discriminator return plus caller-owned typed success/payload out locations. The compiler chooses one canonical form per boundary class and verifies adapters.

No dynamic string identifies a family. No dynamic registry, heap exception, ambient flag, hidden allocation, or handler lookup is required. Payload storage is bounded at compile time and owned by the caller/frame; cleanup is represented in CFG. The top-level entry point returns an explicit compiled `EntryOutcome` that JIT/CLI/native wrappers inspect directly.

### 9.2 Producer contract

Every producer must either:

1. return success and fully initialize the typed success result;
2. return one declared family ordinal and initialize exactly its typed payload, if any; or
3. be classified Unknown/rejected at compile time when no honest adapter exists.

Sentinel normal values, logs, errno-like implicit reads, or later global polling are not outcomes. Existing helpers become ordinary bounded native functions; this architecture introduces no managed scheduler, exception subsystem, or universal allocation regime.

### 9.3 Three migration domains

After the ABI/IR interface freezes, migrate in parallel with disjoint ownership:

1. **Value producers:** checked numeric operations, parsing/conversion helpers admitted by their owners, list/dictionary/container, and matrix helpers.
2. **Resource producers:** file/stdio/resource acquire/read/write/flush/close, snapshot/commit, cleanup/drop, and admitted backpressure helpers.
3. **Execution producers:** generated calls, task create/run/join/settle, native helpers, and FFI adapters.

Each node owns its declarations, implementation, codegen call sites, tests, and structural search. Cross-domain convergence occurs before ambient-state deletion.

## 10. Ambient global error deletion

Only after all three producer domains plus directional-ordering and lazy consumers use explicit outcomes, the Q03 convergence owner deletes:

- runtime error TLS/global flags, message/subcode storage, `RuntimeErrorSnapshot`, and take/set/clear/has/last/match functions;
- `set_runtime_error_once` and every producer call;
- exported `styio_runtime_*error*` ABI and string-family matching;
- codegen runtime-error guards and resource-effect guard-depth state;
- JIT registrations and native-wrapper declarations;
- driver clear/poll/report/event-success logic based on ambient state;
- compatibility fixtures, expected outputs, CMake/CTest/CI entries, and documentation.

Logging state may remain only if it is semantically independent and cannot change control flow or family selection. There is no feature flag or shadow error channel.

## 11. Optimizer rights

### 11.1 Reorder exact once

Required proof: the evaluation still occurs exactly once and preserves value, finite completion, return/termination behavior, access stages, resource versions, numerical rules, cleanup, commit, and publication observations. This right can move an evaluation only across nodes with which the graph/facts prove independence.

### 11.2 Speculate

Required proof: reorder-exact-once plus proven termination/totality for admitted inputs, empty completion set, no fatal/trap, no effect/access, and no allocation/address/identity/lifetime/resource/version/volatile/task/stream observation. Lazy/control demand may be crossed only with this right.

### 11.3 Duplicate

Required proof: speculation-safe plus deterministic equivalent value under the exact numerical contract and no observable evaluation-count, allocation, address identity, lifetime/drop, resource version, or admitted cost consequence. Physical rematerialization never changes logical exact-once semantics.

### 11.4 Elide

Required proof: total, terminating, completion-free, fatal/trap-free, effect/access-free, identity/lifetime/resource-unobservable and unused under the exact parent contract. A strict child cannot be erased merely because its result register is unused if evaluating it may complete, diverge, trap, allocate observably, or run cleanup.

### 11.5 Consumers and backend mapping

- CSE requires equivalence, appropriate duplicate/elide rights, and the same immutable/resource version.
- Constant folding uses the same operation descriptor and completion edge as generated execution.
- Inlining preserves ENF values and facts; it does not clone evaluations.
- Reassociation/vectorization/fusion require numerical equivalence and all affected ordering rights.
- Resource-write coalescing requires the focused protocol's same-final-state proof and no intervening observation barrier.
- LLVM memory, `willreturn`, `noreturn`, `speculatable`, no-free/no-sync, and fast-math attributes are mapped only from facts that meet LLVM's stronger contract. Checked integer Add never gains `nsw`/`nuw` as a replacement for its completion edge; strict floating operations never gain forbidden reassociation/fast-math.

## 12. Caching, isolation, and concurrency

Cache keys include semantic node/call-instance identity, canonical `OperationSummary`, effect-row/return/observability fingerprints, relevant resource/endpoint catalog versions, and compiler policy version. Cache entries never contain raw addresses or mutable AST fields. A changed Q01/Q02/Q05 catalog or native/resource declaration invalidates dependent facts, graphs, outcome layouts, and optimizer decisions.

Compilation sessions own dense IDs, graph storage, ready queues, SCC worklists, and outcome layouts. Shared immutable catalogs may be process-wide only with deterministic content-addressed identity and synchronization. Outcome state is frame/caller-owned; there is no global/TLS semantic state. Concurrent compilation and execution cannot cross-contaminate facts, diagnostics, or outcomes.

## 13. Error handling

Compiler construction errors use typed compiler diagnostics and abort the invalid pipeline before codegen. Graph cycles, unordered order-sensitive siblings, Unknown composition, ABI layout mismatch, or verifier failures never fall back to source order, `Undefined`, `i64`, sentinel values, or ambient errors.

Operation-declared completions use explicit outcome successors. Fatal host conditions that cannot be represented by an admitted family remain outside ordinary success and are modeled conservatively for totality/optimization; adapters must not relabel them as a safe-pure success.

## 14. Module ownership and migration order

| Layer | Single authority after migration | Existing hot paths to converge |
|---|---|---|
| Facts/Sema | one `EvaluationFactTable` and catalog/fixed-point implementation | `src/StyioSema`, session/type/call-instance facts, IDE/cache adapters |
| Frontend/ENF | canonical Q01 syntax nodes plus one exact-once normalizer | `src/StyioParser`, `src/StyioAST`, visitors/clones/repr, AST-to-IR lowering |
| Graph/IR | one typed evaluation DAG, scheduler, CFG schema, and verifier | `src/StyioIR`, `src/StyioResourceTopology`, `src/StyioLowering` |
| Backend | verified IR/fact consumer only | `src/StyioCodeGen`, optimizer pipeline, LLVM attribute emission |
| Outcome boundary | compile-time layout plus generated/native adapter convention | `src/StyioIR`/lowering, `src/StyioExtern`, native helpers |
| Execution surfaces | explicit entry outcome | `src/StyioJIT`, `src/main.cpp`, generated native wrapper |

New files may be chosen during implementation after inspecting concurrent work, but they cannot duplicate an existing owner. Shared hot files receive one cutover owner; producer-domain changes use disjoint file ownership and rebase around concurrent edits rather than reverting them.

## 15. Delivery gates

### Gate A — Source-unreachable foundations

Requirements/evidence/validation/architecture, fact domain, Sema publication, ENF, DAG/IR, verifier, and Outcome layout can land source-unreachable or behind internal construction sequencing only when they do not create dual user semantics. Unit/property tests must pass.

### Gate B — Product cutover train

After the Q01 canonical interface receipt exists, directional ordering integration, lazy CFG, all three explicit-outcome producer domains, codegen/JIT/CLI consumers, and ambient-error deletion converge without an intermediate release. There is no Q03 compatibility adapter, feature flag, old Q03 visitor, old ABI, or positive legacy fixture.

### Gate C — Optimizer enablement

Four-right optimization is enabled only after verifier coverage is complete and ambient outcome side channels are absent. Each transformation has right-specific negative tests.

### Gate D — Structural zero

Final validation deletes every obsolete source path, test, fixture, expected file, CMake/CTest/CI gate, generated index entry, and runbook statement. Full correctness, determinism, fuzz/stress, supported sanitizer/security, performance, documentation, lifecycle, local-information, and Better Plan checks pass on the same revision before Q03-F is marked complete.
