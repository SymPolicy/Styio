# Styio Functional Evaluation and Effect Ordering Validation

**Purpose:** Define the executable, structural, performance, and non-overreach evidence required to accept every Q03-F requirement before implementation begins.

**Last updated:** 2026-07-20

## Validation principles

1. Source-observable behavior, Sema facts, DAG edges, IR/CFG shape, verifier results, generated/native outcomes, and optimized execution must agree on one source revision.
2. Traversal order is not an oracle. Tests vary source order and legal topology order, use counters and distinct completion/effect events, and inspect the graph/CFG rather than inferring correctness from one output.
3. Unknown, completion-capable, divergent, identity-observing, allocating, volatile, resource, and native cases need negative tests for every optimization or sibling-composition permission.
4. A source-defined lazy path is accepted only when the unselected path cannot execute, allocate, complete, acquire/drop resources, or mutate a counter at every optimization level.
5. Deletion evidence includes source, tests, fixtures, generated artifacts, docs, runbooks, CMake/CTest registration, and CI gates. A removed implementation may not retain a positive compatibility test.
6. Performance evidence uses fixed checked-in graphs and operation counts. Wall time and memory measurements support, but do not replace, complexity proof.

## Requirement-to-evidence matrix

| Requirement | Executable evidence | Static/structural evidence |
|---|---|---|
| `REQ-FEO-001` | Counter fixtures cover receiver/argument/operand/source/endpoint/index evaluation and resource-method parameters referenced 0/1/N times; parent never runs after child completion. | ENF dump has one definition per logical input; verifier rejects duplicate/missing definitions and AST substitution. |
| `REQ-FEO-002` | Fixed-seed scheduler variants for independent safe-pure siblings produce identical values/completions and no observable event-order assertion; no task/thread event is created. | DAG contains only required data/control edges; docs/tooling do not promise source order or parallelism. |
| `REQ-FEO-003` | Calls, binary ops, composites, indices, and arrow preparations with two effectful/completing/unknown siblings fail with both source ranges; prebound Block variants pass. | Sema golden facts classify Unknown/native/no-normal-return as order-sensitive and contain no fallback order edge. |
| `REQ-FEO-004` | Counter/event fixtures prove Block effect order, later-item stop, no candidate publication on completion, mandatory cleanup execution, and no rollback of prior effects. | CFG has completion-stop and cleanup/publication joins; verifier rejects early publication or bypassed exit obligations. |
| `REQ-FEO-005` | Serialization/IDE tests show stable `EvaluationFacts` and unchanged operation success/completion summaries. | Type/API assertions prove `OperationSummary` has only two owned fields and Q03 facts live separately; no duplicate completion set. |
| `REQ-FEO-006` | Built-in, concrete-call, recursive-SCC, native/FFI, cache, and concurrent-session fixtures produce deterministic summaries/diagnostics under reversed definition/call order. | Summary lattice/fixed-point tests terminate; Unknown is top; consumers reference one immutable Sema side table and stable fingerprints. |
| `REQ-FEO-007` | Deep/nested call, resource-method, composite, and selector fixtures execute each prerequisite once; randomized frontend traversal cannot change results. | Canonical AST/ENF dumps and ownership tests prove unique `ValueRef`s; no child lowering inside unspecified C++ call arguments or parameter AST clone remains. |
| `REQ-FEO-008` | Unit/property tests cover every edge kind, RAW/WAR/WAW, aliases, barriers, explicit HB, cycles, stable tie-breaking, randomized valid schedules, and conflict diagnostics. | Graph builder uses adjacency and per-region version/last-writer/reader-epoch state; complexity review and counters show `O(V+E)` construction/scheduling apart from declared alias queries. |
| `REQ-FEO-009` | Verifier negative corpus mutates summaries, successors, dominance, laziness, definitions, resource versions, cleanup, commit, and publication; every mutation fails closed. | IR schema carries operation/value IDs, facts, normal/completion successors, lazy regions, and joins; backend contains no repair path. |
| `REQ-FEO-010` | Against the Q01 canonical directional node, source/endpoint counters cover pure/effectful/prebound/conflict cases; each prerequisite runs once and transfer starts only after both normal values. | The integration consumes one Q01 node/fact interface and introduces no Q03 parser/AST shape or legacy adapter. Q01's canonicalization/deletion receipt is a prerequisite, not a Q03-owned deletion. |
| `REQ-FEO-011` | Condition, short-circuit, match guard/body, fallback, and settlement tests place counters/completions/resources in every branch and prove selected-only execution at O0/O2/O3. | CFG/IR snapshots show decision then branch then typed merge; no source-defined lazy node lowers by eagerly computing both inputs to `select`. |
| `REQ-FEO-012` | ABI round trips cover success, every finite family ordinal, zero-payload completion, typed payload, nested call propagation, cleanup, and top-level reporting. | Layout/verifier tests prove compile-time bounded discriminators/out locations with no strings, registry, heap exception, TLS/global state, or universal source value. |
| `REQ-FEO-013` | Checked arithmetic, parse, list/dict/container, and matrix success/failure cases compare constant and generated outcomes, including false/zero/empty successes. | Every domain helper signature exposes explicit outcome transport; no sentinel or ambient setter remains in the domain. |
| `REQ-FEO-014` | File/resource acquire/read/write/flush/close, snapshot/commit, cleanup/drop, and backpressure cases cover success, each failure, partial progress, and Block exit. | Resource accesses/outcomes/commit barriers are visible in facts and CFG; no domain helper sets or reads ambient state. |
| `REQ-FEO-015` | Generated call, recursion boundary, task run/join/settle, native success/failure, and FFI adapter tests cover propagation, Unknown rejection, and cleanup. | Call/task/native ABI declarations are explicit; no fabricated `i64`/`Undefined`, string subcode, or ambient state bridge remains. |
| `REQ-FEO-016` | JIT, native wrapper, CLI run/event/diagnostic, settlement, and unhandled top-level completion tests consume explicit outcomes only. | Structural search proves error globals/TLS, snapshots, set/clear/has/last/match APIs, codegen guards, JIT symbols, driver polling, and positive compatibility fixtures absent. |
| `REQ-FEO-017` | Each right has positive and negative transform tests; moving exactly once, early speculation, rematerialization/duplication, CSE, and DCE are exercised independently. | Optimizer logs/IR receipts name the consumed right; facts show completion, totality, identity/allocation/lifetime, resource version, and result-use proof obligations. |
| `REQ-FEO-018` | Folded versus forced-generated-execution checked arithmetic and strict FP compare bits/completions at O0/O2/O3/LTO where supported; volatile/native/resource/lazy cases resist illegal transforms. | LLVM IR audit checks attributes/flags against facts and rejects forbidden fast-math/reassociation/`nsw`/`nuw`/speculation assumptions. |
| `REQ-FEO-019` | Focused suites, full CTest, supported sanitizer/security/fuzz/deep-graph tests, docs gates, and performance corpus pass on one revision. | Structural-zero inventory classifies every Q03-owned search match; obsolete Q03 tests/gates/docs are deleted, the Q01 prerequisite receipt is linked, and Better Plan validates. |

## Focused test suites

Implementation registers focused tests/labels rather than hiding Q03-F only inside monolithic feature fixtures:

- `functional_evaluation_facts` — primitive fact lattice, Unknown, composition, stable IDs/fingerprints, Q01 separation, call SCC fixed points.
- `functional_evaluation_enf_once` — strict prerequisites, 0/1/N parameter use, receiver/source/endpoint/index counters, deep AST normalization.
- `functional_evaluation_dag` — edge derivation, region versions, RAW/WAR/WAW, conflict/cycle diagnostics, stable/random schedulers, complexity counters.
- `functional_evaluation_ir_verify` — typed values, completion successors, dominance, laziness, cleanup/commit/publication, mutation negatives.
- `functional_evaluation_direction` — Q01 canonical-node consumption, independent preparations, prebinding, conflict rejection, and absence of a Q03 compatibility adapter.
- `functional_evaluation_lazy_cfg` — boolean/conditional/match/guard/fallback/settlement selected-only execution and typed merges.
- `functional_evaluation_outcome_abi` — layouts, family ordinals, zero/typed payloads, propagation, adapter boundaries, top-level outcome.
- `functional_evaluation_producer_value` — numeric/parse/container/matrix producer migration.
- `functional_evaluation_producer_resource` — file/resource/cleanup/backpressure producer migration.
- `functional_evaluation_producer_execution` — task/call/native/FFI producer migration.
- `functional_evaluation_optimizer_rights` — separate reorder/speculate/duplicate/elide proofs and backend attributes.
- `functional_evaluation_graph_benchmark` — fixed DAG/SCC/access corpora, operation counts, peak memory, determinism, and regression budgets.

All slices register under the existing `functional_evaluation` feature label and `tests/features/functional_evaluation/` catalog directory. The final receipt uses the repository's actual configured build directory and records the normal configure/build command, focused `ctest --test-dir <build> --output-on-failure -L functional_evaluation`, relevant existing Sema/IR/resource/execution/security/IDE labels, and the repository full test gate. It must not embed a machine-specific build path.

## Exact-once and ordering oracles

Counter helpers must distinguish logical evaluation from physical pure rematerialization. An order-sensitive test input increments a visible counter or appends a typed event exactly when invoked; a pure test input records only harness-side invocation count. Cover:

- callee versus arguments, receiver versus arguments, left versus right operand, composite members, base versus index, source versus endpoint;
- one order-sensitive child plus safe-pure siblings (accepted), two unordered order-sensitive children (rejected), and the same pair prebound in consecutive Block items (accepted);
- a child that succeeds and a child that completes, plus Unknown/native and proven `never` cases;
- resource methods whose receiver/argument appears zero, once, and multiple times in the body;
- source order permutations that preserve the same dependency graph;
- fixed-seed legal topology permutations for safe-pure regions.

No oracle expects a particular physical order for independent safe-pure siblings. The oracle compares values, completion family/payload, resource versions, observable event trace, cleanup, and publication.

## DAG, CFG, and verifier matrix

Graph tests construct minimal witnesses for every edge and mixed graph:

- producer/consumer data and phi joins;
- conditional/match/settlement lazy control;
- Block completion-stop and no-publication paths;
- lexical ordered effects and explicit resource HB;
- RAW, WAR, WAW, read/read freedom, alias Unknown, snapshots, consume, volatile/native barriers;
- ownership/borrow/drop, task join, commit, flush/close, backpressure, and exit publication;
- self-cycle, two-node cycle, long cycle, disconnected components, deep chains, wide fan-in/out, and large region sets.

Mutation tests remove or redirect one edge/successor/fact at a time and require a stable verifier diagnostic. Scheduler tests use a stable ID priority for reproducible output, then a test-only fixed-seed alternate ready-node choice to prove legal no-edge regions have equivalent semantics. The alternate scheduler is test infrastructure, not a language feature or production nondeterminism.

## Lazy and completion oracle

For every lazy construct, each nonselected path contains independently observable combinations of:

- counter increments;
- a finite completion;
- resource acquisition/drop;
- allocation/identity production;
- native/Unknown call;
- strict FP or checked-overflow operation.

Only the selected path may execute. Settlement additionally covers operation success, each named family, safe fallback categories, unhandled propagation, completing recovery, result joins, and surrounding Block stop/publication. Operation and selected recovery each run at most once; success runs no recovery.

## Outcome ABI and producer-domain oracle

The ABI test oracle is generated from the compiler's resolved finite `OperationSummary`, not from dynamic strings. For every concrete layout it verifies discriminator width/range, ordinal mapping, success storage, zero-payload and typed-payload completions, alignment, initialization, cleanup ownership, and propagation through generated and native boundaries.

Each producer inventory is exhaustive for source-reachable helpers. A domain node cannot complete while an owned helper still sets ambient error state, encodes error as a normal sentinel, drops the family/payload, or relies on a later global poll. Cross-domain calls are included at convergence: parse feeding a resource write, file read feeding a container/matrix operation, task completion carrying a native/FFI failure, and cleanup after any of those completions.

## Optimizer rights matrix

| Transformation | Required positive cases | Required blockers |
|---|---|---|
| Reorder exact once | independent total safe values, disjoint immutable resource versions where the protocol proves equivalence | data/control/stop/HB edges, completion, Unknown normal return, strict observable numerical order, alias/version conflict, commit/publication |
| Speculate | total completion-free effect-free identity-unobservable expressions | divide/trap/resource exhaustion, allocation, overflow completion, divergence, volatile/native, lazy branch, resource/version observation |
| Duplicate | cheap stable pure values with no identity/allocation/lifetime/count observation | allocation/address identity, expensive/observable count, FP transformation without equivalence, completion, resource read/version, lifetime/drop |
| Elide | unused total completion-free effect-free identity/resource-unobservable result | strict child needed by parent, completion/trap/divergence, effect, allocation/lifetime, cleanup/commit/publication, volatile/native |

CSE additionally proves expression equivalence and the same immutable/resource version. Resource-write coalescing additionally proves no intervening observation barrier and the same final logical state. LLVM attributes are audited separately from Styio rights because an attribute may express only part of a proof.

## Performance and determinism evidence

Before enabling production scheduling/optimization, record a checked-in corpus with small representative graphs, deep chains, wide pure fan-out, many Block items, many regions, repeated reads, alternating reads/writes, aliases, call SCCs, lazy diamonds, and hostile cycles/conflicts.

The receipt records node/edge/access counts, summary-union operations, alias queries, ready-queue pushes/pops, SCC iterations, verifier visits, compile time, and peak memory. Acceptance requires:

- graph build, scheduling, and verification operation counts proportional to `V + E` plus explicitly measured alias/fixed-point work;
- no pairwise access matrix or repeated scan of all graph edges for each node;
- deterministic facts, diagnostics, graph dumps, specialization/outcome layouts, and generated IR across repeated builds and source-registration permutations;
- no material compile-time or peak-memory regression against the evidence-approved baseline without a documented correctness justification and owner approval.

Elapsed time and sampled memory are regression evidence, never semantic admission gates.

## Cross-lifecycle directional prerequisite audit

Before `REQ-FEO-010` integration starts, record the Q01 receipt proving that one canonical directional/settlement interface exists and legacy parser/AST/IR direction forks are no longer source reachable. Searches for `FlowBindAST`, `CreateAwait`, `SIOFlowBind`, task-target/await/pull fields, arrow-owned `ResourceRedirectAST`, and parser target-shape branches are routed to Q01 if any remain. Q03-F neither deletes them nor constructs an adapter around them.

## Q03-F structural removal gates

The final receipt includes scoped `rg` searches plus manual classification for at least:

- any Q03-owned directional compatibility adapter, alternate settlement AST/matcher, or ENF special case for a legacy Q01 node;
- source-reachable eager `SGFallback`, `SGWaveMerge`, `SGGuardSelect`, or equivalent both-input-before-select paths;
- `set_error_once`, `set_runtime_error_once`, `RuntimeErrorSnapshot`, runtime error flags/messages/subcodes, has/last/clear/match/take APIs, string family matching, codegen guard helpers, resource-effect guard depth, JIT symbols, driver/native-wrapper polling;
- orphan `EffectKind`, `ir_expr_is_speculatable`, node-kind purity shortcuts, unconditional optimizer attributes, and prohibited fast-math/reassociation paths;
- undefined/`i64` backend repair and any sentinel that encodes completion as a normal value;
- obsolete positive fixtures, expected outputs, CMake/CTest entries, docs, runbooks, generated indexes, and CI checks.

Search terms are evidence starters. Every remaining match receives an owner and semantic classification; aliases and wrappers are inspected rather than excused by naming.

## Standing commands and final gate

The final node records actual command lines and outcomes for the repository's configure/build, focused and full CTest suites, supported sanitizer/security/fuzz checks, syntax convergence, documentation audit/lifecycle, local-information checks, and:

```text
python scripts/manifest_tool.py validate docs/plan
```

The final gate passes only when all `REQ-FEO-*` rows have receipts on the same revision, all implementation and deletion paths converge, no compatibility route or obsolete test/gate remains, performance/determinism evidence is recorded, and adjacent-owner findings are routed without being implemented here.
