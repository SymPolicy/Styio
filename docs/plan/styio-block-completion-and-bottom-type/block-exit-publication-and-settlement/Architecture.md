# Styio Block Exit Publication and Settlement - Architecture

**Purpose:** Define the compiler-owned exit graph, verified schedule, fixed failure layout, and module boundaries for frozen `O05-Q06`.

**Plan:** `styio-block-completion-and-bottom-type/block-exit-publication-and-settlement`

**Last updated:** 2026-07-16

## 1. Layering and module responsibilities

| Layer/module | Responsibility |
|---|---|
| `StyioSema` | Classify the exit reason and prove source-level ownership, borrow, effect, bound, and candidate constraints. It does not choose native storage or execute cleanup. |
| `StyioResourceTopology/ExitActionGraph.*` | Build the per-Block semantic action DAG from lexical, ownership, borrow/capture, commit/HB, hook, child, and candidate facts; deduplicate edges; detect cycles; compute a stable schedule and semantic ordinals. |
| `StyioIR/GenIR/SGExit.hpp` | Consume the parent's already normalized lexical Block-result form and represent the sealed candidate, typed action descriptors, dependencies/schedule, exit reason, fixed failure-layout descriptor, and final sink. Source yield spelling is not retained. |
| `StyioIR/ExitProtocolVerifier.*` | Verify action kinds, bounds, edge validity, ownership uniqueness, commit/abort legality, stable ordinals, fixed slots, and final-sink consistency. Existing `Verifier.cpp` delegates this responsibility rather than accumulating it. |
| `StyioLowering/ExitProtocolLowering.*` | Adapt Sema/RTG facts to verified exit IR and lower every source exit edge to the one protocol. It does not re-infer dependencies in codegen. |
| `StyioCodeGen/ExitProtocolCodeGen.*` | Emit candidate storage, direct native CFG, action calls, fixed status/payload slots, deterministic concurrency batches, direct effect-handler branches, and the final publish/branch/propagate/fatal sink. |
| Existing `StyioExtern` and accepted resource/task hooks | Execute concrete join, flush, release, close, discard, or commit operations and return their declared fixed status/payload. A later admitted feature may add its own hook through the same verified boundary. Hooks do not own graph ordering or global aggregation. |
| Tests/docs/tooling | Prove deterministic behavior and mirror the semantic contract without adding grammar. |

Dependency direction is `Sema + ResourceTopology -> verified StyioIR -> Lowering -> CodeGen -> native hooks`. Runtime support never reaches back into semantic graph construction, and no global mutable object becomes the exit protocol authority.

The parent Block-completion plan guarantees that `<| expr` and
`|<| expr |;` have already become the same typed current-Block result before
this layer. The exit protocol cannot branch on authored spelling or create a
second inline-specific action graph.

## 2. Exit action graph data model

Conceptually, each Block owns:

```text
ActionId = dense u32 index

ExitAction = {
  id: ActionId,
  kind: StabilizeCandidate | JoinChild | CompleteAdmittedRetainer |
        PrepareState | MergeState | LogicalCommit | AbortPending |
        Flush | ReleaseHook | CloseDrop | DiscardCandidate |
        SealFailures | Publish | Branch | Propagate | Fatal,
  outcome: Total | Fallible(CompletionFamily, SlotId) | Fatal,
  lexical_depth: u32,
  registration_ordinal: u32,
  stable_source_id: SourceNodeId,
  owner: optional ResourceNodeId
}

ExitActionGraph = {
  actions: dense vector<ExitAction>,
  successors: compact adjacency lists,
  indegree: vector<u32>,
  schedule: vector<ActionId>,
  batch_boundaries: vector<u32>,
  failure_layout: ExitFailureLayout
}
```

Dense indices keep adjacency, indegree, ordinal, and slot lookup compact and deterministic. Pointer identity and hash iteration are excluded from ordering. Edge insertion deduplicates `(from,to)` before indegree calculation; duplicate semantic evidence may retain diagnostic provenance but cannot execute an action twice.

## 3. Graph construction and stable scheduling

The builder adds edges from:

- inner lexical scope before outer scope;
- move/candidate ownership and last-borrow constraints;
- owned-child join before release of anything the child can reach;
- terminal completion of any separately admitted frame-retaining feature before captured-frame release;
- pending prepare/merge before logical commit;
- logical commit before required flush, release hook, or close when the family contract says so;
- explicit RTG commit and happens-before relations;
- resource-family hook dependencies;
- all non-transferable obligations before the final publish/branch/propagate sink.

Use Kahn topological scheduling with a deterministic ready queue. The comparison key is:

```text
higher lexical_depth,
higher registration_ordinal,
lower stable_source_id,
lower ActionId
```

Dependency edges always override this reverse-registration default. The builder records ready sets as concurrency batches only when no dependency path or declared shared-state conflict exists between their members. Semantic ordinals are assigned before execution from the stable schedule. Complexity is O(V + E) for graph construction and O(V log V + E) for stable scheduling; IR and codegen consume the cached result once.

A cycle is an error with the smallest source-located causal cycle reconstructed from predecessor information. Codegen never breaks a cycle heuristically.

## 4. Candidate and exit-reason state machine

```text
Running -> CandidateReady -> Settling -> Published(T)
                                  \-> Failed(effect state)
                                  \-> Fatal
```

The candidate is evaluated once into epilogue-owned storage before actions that can destroy the source scope. A moved owner is removed from the dying cleanup set. A borrow into an owner that settlement will destroy is rejected. Epilogue entry seals registration.

Successful control exits (`}`, `<|`, `break`, `continue`) select the current logical-frame commit subgraph. Typed failure/cancellation selects abort only for unpublished pending state. Neither branch promises rollback of an earlier barrier or external I/O. The final sink publishes a value, branches to the control target, propagates fixed typed failure state, or performs the declared fatal termination.

## 5. Fixed failure layout

The IR owns a compile-time layout descriptor, not a runtime object:

```text
ExitFailureLayout<N> = {
  primary_ordinal: optional ordinal,
  failed_bits: fixed_bitset<N>,
  handled_bits: fixed_bitset<N>,
  causal_status: fixed status fields,
  payload_slots: heterogeneous inline slots fixed by CompletionFamily
}
```

`N` is the proven number or declared bound of typed fallible obligations. `N == 0` emits no layout. Recording a failure is a direct store into its known slot plus a bit set. Zero-payload effects need only status bits. Liveness may reuse slots whose lifetimes cannot overlap, but the verifier must prove the reuse and preserve stable ordinal identity.

If the body already failed, that ordinal stays primary. Otherwise the lowest failed semantic ordinal is primary. Later failures remain secondary. A bounded child owns a contiguous segment; the parent scans child segments by stable spawn ordinal and preserves local causal order. Expected sibling cancellation caused by an existing primary is causal status, while unexpected cancellation, timeout, refusal to terminate, join failure, or cancellation-handler failure uses its own declared typed slot.

The recovery edge is a compiler-emitted direct branch selected from the primary nominal completion family after the ledger is sealed. Secondary completions cannot hijack routing or erase the primary. Recovery requires explicit settlement of all present bits and an explicit replacement candidate.

## 6. Total, fallible, and fatal actions

These are internal verifier classes, not source syntax:

- `Total`: a compiler/resource contract proves no typed recoverable failure under valid invariants.
- `Fallible(E)`: the action has a known nominal completion family and fixed slot.
- `Fatal`: native execution cannot safely continue; no ordinary typed recovery is guaranteed.

An ignored return code, logging side channel, or forced abort does not convert a physical failure into `Total`. Resource-family adapters must expose an honest status contract. Fatal traps, verified-bound violations that make execution impossible, and process inability to continue are outside the typed cleanup guarantee.

## 7. No-runtime and source-surface firewall

Generated code uses only stack or already preallocated contiguous storage. It performs no heap allocation, exception-object mutation, dynamic handler lookup, reflection, or global failure-list append. `StyioRuntime` may continue to provide support logging for diagnostics, but it is not canonical typed state and cannot decide primary/secondary failure.

No parser, tokenizer, AST surface, or authored exception type is added. Source-visible consequences are limited to honest existing effect typing, stable diagnostics, ordinary result publication timing, and fatal termination behavior. Decided D02 excludes ordinary value fallback and cannot be reopened by this implementation.

## 8. Complete migration boundary

The implementation replaces construct-specific cleanup/commit control flow with `ExitProtocolLowering` and `ExitProtocolCodeGen`. Existing files keep only thin adapters to the new single authority. Family-specific hooks remain concrete actions but no longer order themselves globally. The migration removes default-return repair and first-error-only canonical propagation from exit paths; no legacy or fallback epilogue remains.

## 9. Deliberate patterns

| Pattern | Use | Design value |
|---|---|---|
| Typed DAG plus stable Kahn schedule | Cross-resource/lifetime exit ordering | Expresses real dependencies while preserving predictable reverse-registration behavior for independent actions. |
| Dense IDs and compact adjacency | Per-Block compiler graph | Reduces allocation, hashing, and repeated lookups; makes ordinals and diagnostics stable. |
| IR verifier firewall | Graph/layout/final-sink correctness | Ensures codegen consumes a complete proof and never guesses missing semantics. |
| Fixed frame-owned failure algebra | Multiple typed failures | Preserves evidence without a runtime, heap list, dynamic dispatch, or scheduler-dependent order. |
| Adapter per native hook | Accepted resource/task operations and later conditional integrations | Keeps concrete I/O or lifecycle behavior outside graph scheduling and gives each admitted action one honest status contract. |
| Pattern-free source syntax | User language surface | No new authored mechanism is needed; the complexity belongs entirely to compiler correctness. |
