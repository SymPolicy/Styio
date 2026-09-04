# PLAN-005 Deliver observable topology delta, lineage, and queries

Phase: ready · Revision: unsealed

This document is a render-only projection of `Plan.json`. Edit `Plan.json`; never edit this file.

## Intent

**Goal**: Provide producer-owned incremental delta, explicit lineage, and bounded evidence-bearing queries over published observable topology snapshots.

**In scope**
- Parent-child snapshot contract; canonical delta generation and replay; field-level changes; explicit producer evidence for rename, move, split, and merge lineage; dependency-driven invalidation; bounded query indexes and caches; why, path, effect, ownership, failure, and impact queries; capability negotiation; producer and consumer fixtures.

**Out of scope**
- Runtime instances, scheduler hooks, Vityo presentation, intent policy, heuristic text similarity, unbounded history, telemetry storage, private LSP extensions, and unrelated compiler caches.

**Success**
- Applying a delta to its declared parent reconstructs the canonical target snapshot, query answers match full-snapshot traversal, unrelated edits stay outside the affected closure, lineage exists only with producer evidence, and every cache has an owner, key, invalidation rule, and bound.

**Risk boundary**
- Do not infer lineage from source text, positions, addresses, or timestamps; hide the protocol in unspecified LSP fields; retain unlimited snapshots; weaken completeness semantics; or let cache state become protocol truth.

## Decisions

Dossier status: not_required

No non-discoverable user decision was required.

### Observed repository facts

- none

## Requirements

| Code | Statement | Sources |
| --- | --- | --- |
| REQ-001 | This future stage remains unapproved and must not begin implementation or authorization until PLAN-004 has completed with green full regression and its qualified, deterministic, privacy-safe producer fixtures are accepted. | user-request, sequencing-context |
| REQ-002 | Every target snapshot remains immutable, names its exact parent when known, and is related to that parent only by a producer-created delta and explicit lineage records; applying or querying never mutates either snapshot. | user-request, observable-language-contract |
| REQ-003 | For every valid parent A and child B, delta generation is deterministic and `apply(A, delta(A, B))` reconstructs the canonical B exactly, while a wrong parent, malformed operation, unsupported required capability, or target mismatch fails before publication. | user-request |
| REQ-004 | Delta distinguishes additions, removals, and sorted field replacements for every S1 public snapshot category, including snapshot metadata, nodes, edges, facts, diagnostics, source anchors, evidence, and lineage, without turning identity-field changes into silent mutation. | user-request, plan-intent |
| REQ-005 | Rename, move, split, and merge lineage is published only when the compiler-side producer supplies structured rule-versioned evidence over subjects present in the parent and target; absent evidence means unknown, never a consumer or text-similarity inference. | user-request |
| REQ-006 | A separately versioned public query contract provides bounded lookup, dependencies, dependents, effects, ownership, mutation, failure, task or stream scope, impact, canonical path, lineage, and why queries whose answers are exactly derivable from the selected immutable snapshot. | user-request, observable-language-contract |
| REQ-007 | Every traversal requires finite result, depth, visited-record, and evidence budgets, returns canonical ordering plus explicit truncation, and never allocates or walks beyond the negotiated ceilings. | user-request |
| REQ-008 | Query and delta results preserve producer evidence, source-anchor redaction, snapshot completeness, and unknown or partial status so missing facts are never presented as proven absence. | user-request, observable-language-contract |
| REQ-009 | One explicitly scoped observable service owns all retained snapshots, deltas, query indexes, dependency metadata, byte accounting, and eviction; no process-global, disk-persistent, or unbounded history or result cache is introduced. | user-request |
| REQ-010 | Delta record keys seed exact reverse-dependency invalidation for derived index shards, unchanged shards remain reusable, and cache loss or rebuild never changes public answers or becomes protocol truth. | user-request, plan-intent |
| REQ-011 | Snapshot schema, delta schema, lineage capability, and query protocol remain independently versioned; negotiation selects only a mutually supported incubating version and optional capability intersection, and rejects unknown required capabilities with a machine-readable reason. | user-request, observable-language-contract |
| REQ-012 | Parent eviction, cache absence, incomplete snapshots, exhausted query budgets, unsupported versions or capabilities, and invalid deltas have explicit bounded outcomes such as full-snapshot-required, partial, truncated, unsupported, or invalid; none silently fabricates lineage or stale answers. | user-request |
| REQ-013 | Delta, lineage, queries, fixtures, errors, counters, and caches use only the accepted S1 public snapshot model and contain no AST, IR, compiler object layout, pointer, machine path, source text, raw value, credential, environment content, backend runtime record, or consumer-private extension. | user-request, repository-contract |
| REQ-014 | Ordinary check, build, lowering, code generation, runtime, scheduler, IDE, and LSP behavior remains unchanged; a fixture consumer can deserialize, reconstruct, negotiate, and query without linking ResourceTopology, Sema, Lowering, IDE, LSP, runtime, or LLVM implementation libraries. | user-request, repository-contract |
| REQ-015 | Producer and consumer fixtures cover unchanged, add, remove, field-change, rename, move, split, merge, partial, eviction, unsupported, and malformed cases, with reconstruction and indexed-versus-reference-query properties as executable oracles. | user-request |
| REQ-016 | Publish a privacy-safe handoff to the external benchmark owner for full-versus-delta size, generation and apply work, cold and warm bounded queries, invalidated and reused shards, retained bytes, and peak memory, without adding local performance thresholds or claiming improvement before controlled baselines exist. | user-request, repository-benchmark-contract |
| REQ-017 | Do not add runtime or scheduler correlation, Vityo implementation, heuristic lineage, private LSP or compiler-internal query surfaces, unbounded history or cache, policy evaluation, execution or event replay, telemetry storage, source syntax, or unrelated behavior. | user-request |

## Architecture

One mutually independent Task forms the S2 parallel frontier because the public delta and lineage model, bounded query semantics, cache ownership, invalidation, fixtures, negotiation, and documentation are one compatibility contract. Inside it, delta-lineage and query Nodes branch after the accepted S1 gate, their focused fixtures branch again, and the bounded service, build integration, benchmark handoff, contracts, and final verification join only their real predecessors.

- Treat PLAN-004 as a hard predecessor outside this Plan rather than silently absorbing unfinished snapshot work. S2 consumes the accepted qualified `Snapshot` value, canonical serialization, producer fixture corpus, record ordering, completeness, redaction, and capability metadata. If those fixtures are not green, PLAN-005 stays a draft and no S2 fallback snapshot producer is built.
- Extend the independent public `StyioObservable` service boundary established by S1. Delta, query, and retention code may include S1 public snapshot headers but must not include ResourceTopology, AST, parser, Sema, Lowering, IDE, LSP, runtime, scheduler, LLVM, or consumer-product headers. The compiler-side S1 adapter remains the only owner allowed to translate the validated topology artifact into public facts.
- Give each S1 record category a typed canonical identity tuple made only from its producer-owned immutable semantic key fields. Node identity is its persistent site ID; edges, facts, diagnostics, anchors, and evidence use their S1 subject, kind or role, producer rule, and semantic slot or evidence ID. Identity tuples are compared directly rather than replaced by another digest. A changed identity tuple is remove-plus-add; only non-key fields may appear in a field-replacement operation.
- Generate each category delta by a two-pointer merge over S1's canonical sorted records. Emit additions, removals, and field replacements in category, record-key, then field order. This is deterministic, runs in linear merge time plus changed-field emission, avoids whole-snapshot hash maps, and preserves unknown additive fields according to the negotiated S1 compatibility rule.
- Apply a delta transactionally: verify the exact base snapshot ID, schema major, required capabilities, target metadata, unique sorted operations, referenced keys, and before-values; merge operations into new category arrays; validate cross-record references and completeness once; canonicalize with the S1 serializer; then require the reconstructed snapshot ID and bytes to equal the declared target. The parent remains untouched and no partially applied result is observable.
- Model lineage as target-owned immutable records with `rename`, `move`, `split`, and `merge` relation kinds, prior and target subject sets, producer rule and version, evidence references, and completeness. Enforce one-to-one cardinality for rename and move, one-to-many for split, and many-to-one for merge. Every prior subject must exist in A, every target subject in B, and every evidence reference must resolve. Same-ID continuity requires no speculative lineage record; no evidence means no claim.
- Keep queries as pure functions of one selected snapshot. A simple canonical full-snapshot reference evaluator defines semantics; the production index must return the same ordered records and evidence closure. Direct lookup and filtered adjacency cover nodes and edges; impact is a caller-selected reverse closure over published dependency relation kinds; canonical path is bounded breadth-first search with canonical tie-breaking; why is a bounded walk of the published evidence DAG. Query code must not infer a relation absent from public facts.
- Return a common query envelope containing snapshot ID, negotiated versions and capabilities, completeness, canonical results, visited count, and explicit `complete`, `partial`, or `truncated` status. A negative answer is conclusive only when the relevant snapshot capability and completeness permit it.
- Bind each `ObservableTopologyService` instance to one qualified S1 identity scope. It owns the current and immediate-parent snapshots, at most one parent-to-current delta, immutable per-subject index shards, reverse dependency keys, and byte counters. It caches indexes, not final query answers, so no cross-request result lifecycle or invalidation protocol is needed.
- Build index shards as sorted vectors of stable record ordinals and immutable adjacency or evidence buckets. A new child index merge-reuses a prior bucket only when none of its recorded dependency keys intersects the delta seed set; otherwise rebuild that bucket from B. Evidence dependencies propagate through the finite published evidence DAG. Eviction drops the snapshot, its index shards, dependent delta, and all reverse-dependency metadata together.
- Cache failure degrades to the bounded reference evaluator or a fresh index build over the retained snapshot. An unavailable parent returns `full_snapshot_required`; a retained but incomplete snapshot returns partial answers; a query ceiling returns a deterministic prefix marked truncated. Invalid or unsupported input never changes the service's current state.
- Negotiate snapshot, delta, lineage, and query contracts independently. Match the same major version and highest mutually supported minor, intersect optional capabilities, and reject any required capability outside the intersection. Keep S2 schemas incubating at `0.1`; unknown additive fields follow S1 preservation rules, while unknown critical fields or operations are required capabilities and fail closed.
- Defaults are explicit rather than ambient: query requests default to 256 results, depth 16, 4096 visited records, and 1024 evidence records, with public hard ceilings of 4096 results, depth 64, 65536 visited records, and 8192 evidence records. Service retention defaults to two snapshots, one delta, 64 MiB total snapshot bytes, and 64 MiB total derived-index bytes; construction accepts smaller finite limits, and oversized publication reports a resource limit without affecting compilation or the prior retained state.
- Publish only path-free count and byte statistics needed by the external benchmark harness: input and changed records, delta bytes, visited records, reused or rebuilt shards, and retained bytes. Wall time, CPU, allocation, and peak memory remain measurements made by `styio-benchmark`, not hidden timers or performance claims in the compiler checkout.
- Update only the observable-language SSOT, the public service manifest and module README, the Sema, Test Quality, Performance, and Docs/Ecosystem runbooks, the test catalog, benchmark handoff text, generated documentation statistics, and generated plan index. Do not edit a Vityo repository, LSP protocol, runtime contract, source-language SSOT, or the long-term evolution archive.
- The open-source comparison is deliberately narrow: use the prior-result/full-fallback shape proven by LSP semantic-token deltas and the dependency-key/reuse principle proven by Salsa-style incremental queries, but do not import either protocol, framework, revision database, dynamic memo graph, cancellation model, or dependency. S1 canonical arrays plus typed keys, linear merge, immutable shards, and bounded BFS are sufficient for the current contract.

## Tasks

Every Task belongs to the same mutually independent parallel frontier.

### TASK-001 observable-delta-lineage-query-contract

Worker: general · Tier: complex · Workload: heavy · Verification: code · Frontier: parallel

Outcome: After accepted S1 producer fixtures, Styio exposes deterministic producer-owned snapshot deltas and evidence-backed lineage plus bounded public queries with explicit negotiation, retention, invalidation, degradation, privacy, consumer isolation, and benchmark handoff, while ordinary compiler and runtime behavior remains unchanged.

Risks: quality, performance, privacy, shared_resource
Requirements: REQ-001, REQ-002, REQ-003, REQ-004, REQ-005, REQ-006, REQ-007, REQ-008, REQ-009, REQ-010, REQ-011, REQ-012, REQ-013, REQ-014, REQ-015, REQ-016, REQ-017
Writes: src/StyioServices/StyioObservable/Delta.hpp, src/StyioServices/StyioObservable/Delta.cpp, src/StyioServices/StyioObservable/Query.hpp, src/StyioServices/StyioObservable/Query.cpp, src/StyioServices/StyioObservable/Service.hpp, src/StyioServices/StyioObservable/Service.cpp, src/StyioServices/StyioObservable/README.md, src/cmake/StyioServicesSources.cmake, src/CMakeLists.txt, src/StyioServices/README.md, src/StyioServices/MANIFEST.md, tests/observable_topology_delta_test.cpp, tests/observable_topology_query_test.cpp, tests/observable_topology_service_test.cpp, tests/observable_topology_consumer_test.cpp, tests/fixtures/observable-topology, tests/CMakeLists.txt, docs/design/Styio-Observable-Language.md, docs/design/performance-testing.md, docs/teams/SEMA-IR-RUNBOOK.md, docs/teams/TEST-QUALITY-RUNBOOK.md, docs/teams/PERF-STABILITY-RUNBOOK.md, docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md, docs/teams/DOC-STATS.md, workflows/TEST-CATALOG.md, benchmark/README.md, docs/plan/INDEX.md
Exclusive resources: configured CMake and CTest tree build/default, generated documentation and plan indexes and statistics, repository worktree documentation, privacy, and hygiene gates

In scope
- Enforce the completed PLAN-004 fixture and regression gate before touching S2 implementation.
- Add typed public delta operations, producer-only lineage construction and validation, deterministic generation, transactional application, and canonical serialization over accepted S1 snapshots.
- Add separately versioned bounded public query requests, responses, limits, capability negotiation, reference evaluation, canonical path and evidence traversal, and deterministic status semantics.
- Add a per-qualified-scope observable service with current-plus-parent retention, one-delta retention, byte limits, immutable query-index shards, dependency-driven invalidation, shard reuse, eviction, and path-free statistics.
- Add producer-authored and consumer-only fixtures and property tests for reconstruction, lineage evidence, query equivalence, privacy, bounds, negotiation, invalid inputs, cache loss, eviction, and partial snapshots.
- Register the independent public contract library and focused tests without adding a private IDE/LSP endpoint or compiler-internal consumer dependency.
- Update the owning service, observable, testing, performance, benchmark-handoff, and generated documentation required by the new public capability.

Out of scope
- Implementing or repairing S1 snapshot production, canonical serialization, source-anchor redaction, completeness generation, semantic identity, or producer fixtures before their acceptance gate is green.
- Runtime instance IDs, lowering instrumentation, scheduler events, wait causes, cancellation or task correlation, telemetry buffers or stores, OpenTelemetry or Perfetto exporters, and deterministic execution or event replay.
- Vityo code, UI, hosted adapters, agent policy, policy evaluation, counterfactual why-not, heuristic text or position matching, and consumer-authored canonical lineage.
- IDE `SemanticDB` caches, private LSP methods or extension fields, parser or compiler object access, AST/IR/LLVM layout, and a second semantic graph.
- Unbounded snapshots, branches, history, caches, query results, pagination state, disk persistence, distributed caches, cloud services, and unrelated compiler caches.
- New source syntax, changed resource semantics, diagnostics, StyioIR, code generation, runtime behavior, package management, or unrelated refactoring.

Outputs
- OUT-001 Versioned delta operations, typed record keys, lineage relations, validation results, and machine-readable degradation reasons (`src/StyioServices/StyioObservable/Delta.hpp`): Consumers can understand additions, removals, field replacements, parent identity, and producer-evidenced rename, move, split, or merge without compiler internals or heuristic inference.
- OUT-002 Canonical linear-merge generation, transactional application, reference validation, and serialization (`src/StyioServices/StyioObservable/Delta.cpp`): Valid A and B satisfy exact canonical reconstruction while invalid bases or operations expose no partial state.
- OUT-003 Versioned query kinds, finite limits, capability negotiation, result status, completeness, and privacy-safe statistics (`src/StyioServices/StyioObservable/Query.hpp`): Every public query declares bounded work and returns an explicit complete, partial, truncated, unsupported, or invalid outcome.
- OUT-004 Reference evaluator, immutable index shards, canonical lookup, adjacency, impact, path, lineage, and why traversal (`src/StyioServices/StyioObservable/Query.cpp`): Indexed answers are canonical and equivalent to facts and evidence in the selected snapshot, with no inferred semantic relation.
- OUT-005 Per-qualified-scope publication, negotiation, retention limits, query and delta access, and path-free counters (`src/StyioServices/StyioObservable/Service.hpp`): One owner controls the public lifecycle and exposes no global cache, compiler pointer, private LSP state, or unbounded request mode.
- OUT-006 Atomic publication, exact dependency invalidation, immutable shard reuse, eviction, bounded reference fallback, and state-preserving failures (`src/StyioServices/StyioObservable/Service.cpp`): Cache state is an optimization only, unrelated shards survive an edit, and every retained object is covered by count and byte bounds.
- OUT-007 Independent observable contract target and focused test registration (`src/cmake/StyioServicesSources.cmake`): Fixture consumers link the public observable contract without linking frontend, IDE, LSP, runtime, or LLVM implementation libraries.
- OUT-008 Canonical parent, child, delta, and evidence fixture matrix with privacy-safe metadata (`tests/fixtures/observable-topology/manifest.json`): Checked-in cases cover unchanged, add, remove, field change, rename, move, split, merge, partial, malformed, and unsupported behavior without raw source or local information.
- OUT-009 Focused deterministic reconstruction and producer-evidence tests (`tests/observable_topology_delta_test.cpp`): Generation, operation ordering, exact application, identity-change handling, lineage cardinality, evidence resolution, and fail-closed validation are executable claims.
- OUT-010 Full-snapshot reference versus indexed-query property tests (`tests/observable_topology_query_test.cpp`): Every query kind, ordering rule, budget, completeness state, path tie-break, and why closure is checked against canonical fixture facts.
- OUT-011 Retention, byte accounting, dependency invalidation, shard reuse, eviction, fallback, and atomic failure tests (`tests/observable_topology_service_test.cpp`): The service never serves stale data, exceeds declared ownership bounds, or lets cache availability change semantic answers.
- OUT-012 Public-only deserialization, negotiation, delta application, and query tests (`tests/observable_topology_consumer_test.cpp`): A consumer validates all public contracts from fixtures without compiler, IDE, LSP, runtime, or LLVM implementation dependencies.
- OUT-013 Public module usage, version, capability, privacy, limits, errors, and ownership documentation (`src/StyioServices/StyioObservable/README.md`): Embedders receive one documented service boundary and no private transport or consumer-specific semantics.
- OUT-014 Registered public delta, lineage, and query capabilities (`src/StyioServices/MANIFEST.md`): The repository service inventory names the authoritative entry points and keeps LSP and Vityo from becoming alternate contract owners.
- OUT-015 Implemented S2 boundary, evidence, completeness, cache, negotiation, and deferred runtime or consumer work (`docs/design/Styio-Observable-Language.md`): The language SSOT distinguishes static public facts from mutable service caches and future runtime overlays.
- OUT-016 Exact fixture families, counters, measurements, cold and warm modes, privacy rules, and no-threshold status for the external owner (`benchmark/README.md`): `styio-benchmark` can implement controlled S2 measurements without this repository inventing a second workload catalog or asserting unsupported speedups.
- OUT-017 Sema ownership, test properties, performance handoff, and Docs/Ecosystem maintenance rules (`docs/teams/SEMA-IR-RUNBOOK.md`): Future changes preserve producer authority, public isolation, bounded retention, benchmark ownership, and focused evidence.
- OUT-018 Discoverable S1 gate and S2 delta, lineage, query, cache, consumer, privacy, and compatibility commands (`workflows/TEST-CATALOG.md`): Maintainers can rerun the narrow evidence without repeating the complete regression.
- OUT-019 Producer identity, evidence, and public-boundary maintenance rules (`docs/teams/SEMA-IR-RUNBOOK.md`): Future semantic producers preserve evidence authority and never move query or cache ownership into Sema.
- OUT-020 Delta, lineage, query, cache, degradation, and isolation test guidance (`docs/teams/TEST-QUALITY-RUNBOOK.md`): Maintainers preserve the property and fixture oracles instead of replacing them with implementation-shaped snapshots.
- OUT-021 External benchmark ownership and measurement guidance (`docs/teams/PERF-STABILITY-RUNBOOK.md`): Timing, allocation, RSS, baselines, and thresholds remain external while public counters and fixture modes stay comparable.
- OUT-022 Service inventory, SSOT, generated-content, and consumer-boundary maintenance rules (`docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md`): Public capability documentation remains aligned without promoting Vityo, LSP, runtime, or archive prose to authority.
- OUT-023 Regenerated documentation statistics (`docs/teams/DOC-STATS.md`): The final tracked documentation tree and generated size inventory agree.
- OUT-024 Current external S2 measurement route (`docs/design/performance-testing.md`): The compiler checkout describes the handoff and never duplicates external workloads or reports.
- OUT-025 Regenerated tracked Better Plan index (`docs/plan/INDEX.md`): Repository documentation gates index PLAN-005 without hand-editing generated content.

Internal Node graph
- NODE-001 verify-s1-acceptance-gate · after: none · Confirm PLAN-004 tasks and full regression are complete and its producer fixture label proves qualified deterministic privacy-safe snapshots; stop S2 without modifying source if the gate is absent or red.
- NODE-002 capture-public-compatibility-baselines · after: none · Record the accepted S1 fixture bytes and capabilities plus current ResourceTopology, Sema/lowering reuse, compiler diagnostics, IDE/LSP surface, and ordinary language behavior that S2 must not change.
- NODE-003 implement-delta-lineage-contract · after: NODE-001 · Add typed record keys, incubating versions and capabilities, deterministic linear-merge generation, transactional application, producer-only lineage construction, and fail-closed validation over the accepted S1 public snapshot model.
- NODE-004 implement-bounded-query-contract · after: NODE-001 · Add finite query requests and responses, negotiation, reference evaluation, canonical lookup and traversals, completeness propagation, and public hard ceilings over the accepted S1 public snapshot model.
- NODE-005 implement-retention-invalidation-service · after: NODE-003, NODE-004 · Add the per-qualified-scope façade, current-plus-parent and one-delta retention, byte accounting, immutable index shards, reverse dependency keys, exact invalidation, reuse, eviction, bounded fallback, atomic failure, and path-free statistics.
- NODE-006 wire-independent-observable-target · after: NODE-005 · Register the observable contract library from only public snapshot, delta, query, and service sources; keep producer adapters and all compiler, IDE, LSP, runtime, and LLVM implementations outside its link interface.
- NODE-007 prove-delta-lineage-properties · after: NODE-002, NODE-003 · Generate the privacy-safe fixture matrix through producer APIs and add deterministic ordering, exact reconstruction, field-change, wrong-base, malformed-operation, lineage-cardinality, evidence-resolution, and no-heuristic tests.
- NODE-008 prove-query-reference-equivalence · after: NODE-002, NODE-004 · Add table-driven reference-versus-index tests for every query kind, canonical path ties, evidence DAGs, completeness, empty answers, defaults, hard ceilings, truncation, and bounded work counters.
- NODE-009 prove-retention-invalidation-and-degradation · after: NODE-002, NODE-005 · Add focused tests for exact invalidation closure, unaffected-shard reuse, cache-drop equivalence, count and byte eviction, parent loss, oversized publication, incomplete snapshots, and state preservation after invalid input.
- NODE-010 prove-consumer-isolation-and-negotiation · after: NODE-006, NODE-007 · Add a consumer-only target that reads checked-in fixtures, negotiates compatible versions and capabilities, applies deltas, runs queries, and rejects unsupported critical input without internal headers or link dependencies.
- NODE-011 integrate-focused-test-targets · after: NODE-008, NODE-009, NODE-010 · Register separate producer, delta, query, service, and consumer tests with observable snapshot, delta, query, and service labels and no duplicate complete-regression wrapper.
- NODE-012 publish-external-benchmark-handoff · after: NODE-005, NODE-007, NODE-008 · Document the external workload and measurement matrix using the accepted fixtures and path-free service counters, including cold full scan, cold index, warm retained query, small-delta invalidation, full fallback, and retention pressure, with no local threshold or external-repository edit.
- NODE-013 update-owning-contracts-and-runbooks · after: NODE-011, NODE-012 · Register the public service, mark S2 implemented only after its focused evidence is green, document producer and cache ownership plus exclusions, add exact test commands and benchmark handoff, refresh generated statistics, and regenerate the plan index without editing source-language, LSP, runtime, Vityo, or archive authority.
- NODE-014 verify-focused-observable-closure · after: NODE-013 · Build the independent observable and affected compatibility targets; run the S1 gate and S2 property, consumer, resource-topology, Sema/lowering, language, architecture, docs, privacy, hygiene, and diff checks once.

Design
- approach
  - Treat the accepted S1 public snapshot types and canonical serializer as the only semantic input. Add no second graph and never rebuild facts from labels, syntax text, pointers, private HIR, compiler diagnostics, or LSP state.
  - Define record identity as transparent typed tuples of existing producer-owned key fields. Validate uniqueness per category before delta generation or application. Never derive a new identity from an entire serialized record, source location, textual similarity, timestamp, or allocation order.
  - Use one canonical merge-diff implementation shared by producer generation tests and service publication. For equal keys compare each versioned non-key field; emit only changed fields with expected before and target values. For unequal keys emit remove or add. Store target envelope metadata once rather than duplicating it in operations.
  - Apply into temporary sorted category vectors and publish only after the complete S1 snapshot validator and canonical serializer accept the target. Exact parent and target IDs are sufficient integrity guards; do not add redundant whole-file checksums, salts, repair probes, or alternate application paths.
  - Keep lineage construction behind the compiler-side producer API. Validate relation cardinality, parent and target membership, rule version, and evidence closure at construction and again on deserialization. Consumers may display or query records but cannot promote hints into canonical lineage.
  - Implement a straightforward full-snapshot evaluator first and retain it as the test oracle and bounded cache-miss path. Build production immutable indexes from the same public records. All result sets sort by canonical public key so unordered container iteration can never affect wire output.
  - Use bounded breadth-first search for canonical path and caller-selected transitive impact, and bounded DAG traversal for why. Stop before exceeding visited, depth, result, or evidence ceilings; return counts and truncation rather than a continuation token or retained traversal state.
  - Represent an index as a sorted subject table whose entries reference immutable adjacency, fact, lineage, and evidence buckets. Each bucket records the source record keys it depends on. Child publication merge-walks the table, shares dependency-clean buckets, and rebuilds only buckets reached from changed keys through the reverse dependency table.
  - Keep no final-answer memoization. This removes result-key explosion, user-specific cache contents, and another invalidation layer while preserving the useful reuse of snapshot indexes.
  - Commit service publication with a temporary candidate state and one swap after snapshot, delta, index, dependency, and retention validation. Errors leave the prior state byte-for-byte observable. There are no elapsed-time deadlines, background eviction threads, or hidden retry loops.
  - Compile the consumer fixture test against the public observable target only. Enforce the source and link boundary with a focused CMake target plus include and link-map assertions; do not claim isolation merely because a test avoids calling internal APIs.
  - Keep the benchmark handoff descriptive and machine-checkable through fixture names and counter fields already exposed by the service. External harnesses own timing, allocation, RSS, reports, baselines, and thresholds; this delivery owns correctness and measurement seams only.
- defaults
  - Query default limits are 256 results, depth 16, 4096 visited records, and 1024 evidence records; public hard ceilings are 4096 results, depth 64, 65536 visited records, and 8192 evidence records.
  - Retention defaults are two immutable snapshots for one qualified scope, one parent-to-current delta, 64 MiB total retained snapshot bytes, 64 MiB total derived-index bytes, and zero cached final query answers.
  - S2 delta, lineage, and query contracts start incubating at 0.1; negotiation requires an equal major version, selects the highest common minor version, intersects optional capabilities, and rejects unknown required capabilities.
  - Missing parent history degrades to full snapshot, missing index degrades to bounded reference evaluation or rebuild, incomplete facts produce partial non-conclusive answers, and budget exhaustion produces a canonical truncated prefix.
- patterns
  - pattern_catalog: refactoring-guru-catalog-22-v1
  - candidate: Facade
  - decision: adopt
  - pressure: Public callers would otherwise need to coordinate version negotiation, exact-parent validation, delta generation or application, retention, index invalidation, eviction, and degradation in the correct order, while those operations share one atomic lifecycle.
  - expected_benefit: One small `ObservableTopologyService` makes ownership and failure atomicity executable, keeps mutable caches behind immutable public values, and lets tests prove that invalid publication cannot leak partial state.
  - simpler_alternative: Independent free functions are sufficient for pure delta and query evaluation and remain the implementation primitives, but they cannot alone own retained state or enforce publication and eviction invariants across calls.
  - application: The façade is limited to one qualified scope and delegates pure work to `Delta` and `Query`; it owns only current and parent snapshots, one delta, index shards, dependency metadata, limits, negotiation state, and counters.
  - costs_and_rejections: The façade adds one coordinating type and lock boundary. Memento is rejected because snapshots are immutable public values rather than hidden restorable compiler state; Observer is rejected because no subscriptions or asynchronous notification exist; Strategy is rejected because the reference evaluator is a fixed oracle or fallback rather than a runtime-selected algorithm family.

Acceptance
- AC-001 covers REQ-001
  - Given PLAN-004 and its qualified producer fixtures
  - When the S2 stage gate is checked before any source Node runs
  - Then every PLAN-004 Task is completed, its full regression is green, and the producer-fixture label is non-empty and passing; otherwise S2 performs no implementation
  - Oracle: `python3 -c "import json; p=json.load(open('docs/plan/observable-static-snapshot/Checkpoints.json', encoding='utf-8')); assert p['tasks'] and all(t['status']=='completed' for t in p['tasks']); assert p['full_regression']['passed'] is True"` and `ctest --test-dir build/default -L '^observable_snapshot_producer$' --output-on-failure --no-tests=error` both exit zero
  - Evidence: command from S1 accepted-producer gate
- AC-002 covers REQ-002, REQ-003, REQ-004, OUT-001, OUT-002, OUT-008, OUT-009
  - Given Every valid canonical parent-child fixture and repeated generation runs
  - When delta is generated and transactionally applied
  - Then operation bytes and ordering are identical across runs, every category reports add, remove, or non-key field replacements correctly, A remains unchanged, and the canonical applied result is byte-for-byte B
  - Oracle: `StyioObservableDelta.CanonicalGenerationIsDeterministic`, `StyioObservableDelta.ApplyReconstructsEveryFixtureExactly`, and `StyioObservableDelta.CoversAllSnapshotRecordCategoriesAndFieldChanges` pass
  - Evidence: command from deterministic delta reconstruction properties
- AC-003 covers REQ-005, REQ-008, REQ-013, REQ-017, OUT-001, OUT-008, OUT-009
  - Given Producer-authored rename, move, split, and merge fixtures plus missing, consumer-suggested, malformed-cardinality, and unresolved-evidence variants
  - When lineage is constructed, serialized, deserialized, and queried
  - Then only valid producer-rule evidence becomes canonical lineage, all subjects and evidence resolve, absent evidence remains unknown, and no text, position, address, timestamp, or consumer hint creates a relation
  - Oracle: `StyioObservableLineage.AcceptsOnlyProducerEvidence`, `StyioObservableLineage.EnforcesRelationCardinalityAndMembership`, and `StyioObservableLineage.AbsenceAndHintsNeverInferRelations` pass
  - Evidence: command from producer lineage authority tests
- AC-004 covers REQ-006, REQ-007, REQ-008, OUT-003, OUT-004, OUT-010
  - Given Complete and partial snapshots covering every public query kind and adversarial canonical path ties
  - When requests run through the full-snapshot reference evaluator and immutable index under default, reduced, and hard-ceiling limits
  - Then both evaluators return identical canonical facts and evidence, BFS chooses the same shortest tie-broken path, partial negative answers are non-conclusive, and counters prove no budget is exceeded
  - Oracle: `StyioObservableQuery.IndexMatchesReferenceForEveryQueryKind`, `StyioObservableQuery.CanonicalPathAndWhyAreBounded`, and `StyioObservableQuery.CompletenessControlsNegativeAnswers` pass
  - Evidence: command from bounded query equivalence properties
- AC-005 covers REQ-009, REQ-010, REQ-012, OUT-005, OUT-006, OUT-011
  - Given A retained parent and child, unrelated and connected delta seeds, cache loss, count and byte pressure, an oversized child, an invalid delta, and an evicted parent request
  - When service publication and queries run
  - Then only dependency-reached shards rebuild, unrelated shard objects are reused, all retained bytes remain within limits, cache loss preserves answers, failures preserve prior state, and missing history returns full-snapshot-required without stale data
  - Oracle: `StyioObservableService.InvalidatesOnlyDependentIndexShards`, `StyioObservableService.RetentionAndEvictionStayBounded`, `StyioObservableService.CacheLossMatchesReference`, and `StyioObservableService.InvalidPublicationPreservesPriorState` pass
  - Evidence: command from cache ownership, invalidation, and degradation tests
- AC-006 covers REQ-011, REQ-012, REQ-013, REQ-014, REQ-015, OUT-007, OUT-012, OUT-013, OUT-014
  - Given Clients with compatible, optional-only, major-incompatible, and unknown-required capability sets plus the checked-in fixture corpus
  - When the consumer-only target negotiates, deserializes, applies, and queries
  - Then it selects the highest common minor and optional intersection, rejects unsupported required input with stable reasons, propagates partial or truncated status, and links only the public observable contract target
  - Oracle: `StyioObservableConsumer.NegotiatesIndependentContracts`, `StyioObservableConsumer.AppliesAndQueriesCheckedFixtures`, and `StyioObservableConsumer.RejectsUnknownRequiredCapabilities` pass, `test -z "$(rg -n 'Styio(ResourceTopology|AST|Parser|Sema|Lowering|IDE|LSP|Runtime)|llvm/' tests/observable_topology_consumer_test.cpp src/StyioServices/StyioObservable || true)"` exits zero, and the consumer target link-map oracle reports none of `styio_frontend_core`, `styio_ide_core`, `styio_runtime_core`, or LLVM
  - Evidence: command from public consumer isolation and negotiation tests
- AC-007 covers REQ-013, REQ-014, REQ-015, REQ-017, OUT-008, OUT-012
  - Given Existing resource topology, Sema/lowering, representative language, IDE, and LSP tests plus S2 fixture privacy scans
  - When the focused compatibility matrix runs
  - Then graph facts and identities, one-artifact reuse, diagnostics, StyioIR/codegen/runtime behavior, IDE/LSP request inventory, and ordinary compilation remain unchanged, while fixtures and errors contain no prohibited local or raw data
  - Oracle: the `resource_topology`, `sema_internal`, `lowering_internal`, `scalar_expressions`, `file_resources`, `state_resources`, `stream_processing`, `task_resources`, `ide`, and `lsp` focused labels pass, `python3 scripts/local-info-leak-gate.py --mode worktree` exits zero, and `! rg -n 'source_text|raw_value|credential|backend_runtime' tests/fixtures/observable-topology` exits zero
  - Evidence: command from compiler compatibility and fixture privacy matrix
- AC-008 covers REQ-016, REQ-017, OUT-013, OUT-014, OUT-015, OUT-016, OUT-017, OUT-019, OUT-020, OUT-021, OUT-022, OUT-023, OUT-024, OUT-018, OUT-025
  - Given Green S2 focused tests and the repository's external benchmark ownership rule
  - When public docs, runbooks, test catalog, benchmark handoff, generated indexes and statistics, and repository gates are checked
  - Then the public service and limits are documented, runtime/Vityo/private-LSP/policy/replay work remains excluded, the benchmark owner receives exact fixtures, counters, modes, and metrics without a local threshold, and generated content is current
  - Oracle: `bash scripts/docs-gate.sh --mode worktree`, `python3 scripts/team-docs-gate.py --mode worktree`, `python3 scripts/docs-audit.py`, `python3 scripts/repo-hygiene-gate.py --mode worktree`, and `git diff --check` all exit zero, while focused `rg` assertions find `delta`, `lineage`, `bounded query`, `full_snapshot_required`, and `styio-benchmark` ownership in their owning docs
  - Evidence: command from observable service, benchmark handoff, and documentation gates

Focused regression
- `cmake --build build/default --target styio styio_resource_topology_test styio_typeinfer_internal_test styio_lowering_internal_test styio_observable_delta_test styio_observable_query_test styio_observable_service_test styio_observable_consumer_test styio_ide_test -j2`
- `python3 -c "import json; p=json.load(open('docs/plan/observable-static-snapshot/Checkpoints.json', encoding='utf-8')); assert p['tasks'] and all(t['status']=='completed' for t in p['tasks']); assert p['full_regression']['passed'] is True"`
- `ctest --test-dir build/default -L '^observable_snapshot_producer$' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -R '^(StyioObservable(Delta|Lineage|Query|Service|Consumer)\.)' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L '^(resource_topology|sema_internal|lowering_internal)$' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L '^(scalar_expressions|file_resources|state_resources|stream_processing|task_resources)$' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L '^(ide|lsp)$' --output-on-failure --no-tests=error`
- `test -z "$(rg -n 'Styio(ResourceTopology|AST|Parser|Sema|Lowering|IDE|LSP|Runtime)|llvm/' tests/observable_topology_consumer_test.cpp src/StyioServices/StyioObservable || true)"`
- `bash scripts/docs-gate.sh --mode worktree`
- `python3 scripts/team-docs-gate.py --mode worktree`
- `python3 scripts/docs-audit.py`
- `python3 scripts/local-info-leak-gate.py --mode worktree`
- `python3 scripts/repo-hygiene-gate.py --mode worktree`
- `git diff --check`
- paths: src/StyioServices/StyioObservable, src/cmake/StyioServicesSources.cmake, src/CMakeLists.txt, src/StyioServices/README.md, src/StyioServices/MANIFEST.md, tests/observable_topology_delta_test.cpp, tests/observable_topology_query_test.cpp, tests/observable_topology_service_test.cpp, tests/observable_topology_consumer_test.cpp, tests/fixtures/observable-topology, tests/CMakeLists.txt, src/StyioResourceTopology, src/StyioSema, src/StyioLowering, docs/design/Styio-Observable-Language.md, docs/design/performance-testing.md, docs/teams/SEMA-IR-RUNBOOK.md, docs/teams/TEST-QUALITY-RUNBOOK.md, docs/teams/PERF-STABILITY-RUNBOOK.md, docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md, docs/teams/DOC-STATS.md, workflows/TEST-CATALOG.md, benchmark/README.md, docs/plan/INDEX.md

## Full regression

Run inside the sole Reviewer session after every repair is integrated.

- `cmake --build build/default -j2`
- `ctest --test-dir build/default --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -R '^architecture_layer_gate$' --output-on-failure --no-tests=error`
- `bash scripts/docs-gate.sh --mode worktree`
- `python3 scripts/local-info-leak-gate.py --mode worktree`
- `python3 scripts/repo-hygiene-gate.py --mode worktree`
- `git diff --check`
- paths: src/StyioServices/StyioObservable, src/StyioServices, src/cmake/StyioServicesSources.cmake, src/CMakeLists.txt, src/StyioResourceTopology, src/StyioSema, src/StyioLowering, tests, docs/design/Styio-Observable-Language.md, docs/design/performance-testing.md, docs/teams, workflows/TEST-CATALOG.md, benchmark/README.md, docs/plan
