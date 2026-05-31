# IM-D5 Stream Concurrency Inventory

**Purpose:** Record the accepted stream-runtime, concurrency, pulse-frame, cross-stream synchronization, and multi-writer merge decisions for IM-D5.

**Last updated:** 2026-05-31

## Scope

IM-D5 owns the language and runtime contract for stream execution over time:

- pulse frame boundaries,
- frame-locked resource snapshots,
- cross-stream synchronization,
- unequal-rate stream behavior,
- stream backpressure scheduling,
- happens-before ordering,
- deterministic multi-writer commit and merge behavior, and
- the observable boundary between deterministic Styio stream code and host/native side effects.

IM-D5 does not redefine the single-resource capability, typestate, fallback, cleanup, or block-entry snapshot rules from [IM-D4-RESOURCE-MANAGEMENT-INVENTORY.md](./IM-D4-RESOURCE-MANAGEMENT-INVENTORY.md). It builds on those rules when more than one stream, task, writer, or resource frame is active at the same time.

## Current State

Styio already has active design and partial implementation surfaces for streams:

- `source >> #(x) => { ... }` is the primary pulse/stream consumer shape.
- `[...]` is the infinite pulse generator.
- `&` is the stream zip token for aligned synchronization.
- Pulse-frame locking is documented as a core Styio design direction.
- Current compiler paths contain parser, sema, lowering, and codegen pieces for selected stream and zip shapes.
- Materialized non-file `list[T]` handles, mixed `@file` / materialized-list
  pairs, and bounded Topology selector snapshots that have already materialized
  as `list[T]` values now have a zip-barrier runtime slice for `i64`, `string`,
  `f64`, `bool`, and `char` list elements, with bounded matrix selector
  snapshots covered as materialized `list[matrix]`: list sides use
  `styio_list_len` / `styio_list_get_*`, file sides read one line per frame,
  finite zip terminates at the shorter file EOF or list length, and the existing
  block/frame commit path runs for each matched pair. `@stdin` now participates
  as the accepted
  standard input line-stream side for finite zip with materialized lists or
  `@file` streams in both source orders; it reads one stdin line per matched
  frame and terminates at stdin EOF or the shorter finite peer. Direct
  `@stdin & @stdin` remains rejected until the stream-driver contract defines
  duplicate consumption of the same external input. This selector-snapshot and
  stdin-stream slice remains ordinary finite zip over already-materialized lists
  or line streams; snapshot joins, queue/timeout policy, pressure observers, and
  multi-writer merge/conflict semantics remain open.
- Unsupported non-iterable or non-materialized-list zip sources, including scalar
  resource selectors such as `@price[-1]` and raw matrix latest selectors such
  as `@bucket[-1]`, now fail closed with the feature-owned JSONL code
  `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE` instead of only the broad
  `STYIO_TYPE_ERROR`. This is an IM-D3 diagnostic refinement, not an expansion
  of accepted stream-driver semantics.
- Ordinary iterator sources that are not iterable, including scalar bindings and
  scalar latest resource selectors such as `@price[-1] >> #(v) => { ... }`, now
  fail closed with the feature-owned JSONL code
  `STYIO_TYPE_ITERATION_UNSUPPORTED_SOURCE` instead of only the broad
  `STYIO_TYPE_ERROR`. This is an IM-D3 diagnostic refinement, not an expansion
  of accepted iterator, selector, or stream-driver semantics.
- Undefined hash-tag iterator sequence routes such as `[1, 2] >> #price` now
  fail closed with the feature-owned JSONL code
  `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED` instead of only the broad
  `STYIO_TYPE_ERROR`. This is also an IM-D3 diagnostic refinement; IM-D5-P1
  still owns the decision to retire hash-tag routes or define them in the stream
  design SSOT before any runtime behavior can be implemented.

The implementation is still incomplete. Multi-stream zip and driver combinations are only partially lowered, unsupported combinations may still end in narrow lowering/codegen paths, and cross-stream sync needs a stable memory-model contract before the remaining implementation can be judged complete.

## Accepted Concurrency Model

Styio's default stream model is a **single-machine deterministic pulse-frame model**.

Accepted decision:

- A stream consumer runs in pulse frames.
- A frame begins when its triggering source and synchronization requirements are satisfied.
- The frame reads committed resource snapshots captured at frame entry.
- Repeated reads of the same frame-locked resource snapshot inside the same frame observe the same value.
- Resource writes inside the frame produce pending effects, deltas, or snapshot updates.
- The frame publishes those effects only at its commit boundary.
- Runtime thread scheduling must not change the observable result of accepted deterministic stream code.
- Go-like arbitrary shared channel concurrency is not the baseline Styio language model.

Go-like channels, actor-like runtimes, and lower-level host scheduling may still be used as implementation techniques or future explicit language features, but they do not define the accepted default memory model.

## Pulse Frame And Block Entry

The IM-D4 block-entry snapshot rule applies inside streams.

Accepted decision:

- `>>`, `=>`, `?=`, active `||>`, and any future accepted block-entry form create a snapshot context when they enter a block.
- A stream frame adds a time boundary around that snapshot context.
- Reads of frame-locked resource state observe the committed state captured for the frame.
- Writes remain pending until the block/frame commit boundary unless an earlier correctness barrier requires settlement.
- Chained stages are multiple frame/snapshot/commit units when each stage enters its own block.

Example:

```styio
prices >> #(p) => {
    p -> @last_price
}
```

The closure runs against a frame snapshot. `@last_price` is not mutated through shared in-place access while the closure is running; the write is committed at the frame/block boundary according to the resource-family rules.

## Cross-Stream Synchronization Modes

Styio has two distinct stream-combination meanings. They must not be collapsed into one hidden runtime behavior.

### Zip Barrier

`&` is a barrier synchronization mode.

Accepted decision:

- `A >> #(a) & B >> #(b) => { ... }` starts a frame only when all participating zip inputs have produced the required item for that frame.
- The frame observes a synchronized tuple of inputs.
- The resulting frame commits once, after the zipped closure body completes.
- No input may be silently dropped by the language-level zip semantics.
- Unequal-rate inputs are handled by pending input queues, pressure, timeout, close, or EOF rules declared by the stream/resource family.
- A finite zip terminates when one finite input reaches normal EOF, unless a resource family declares a different accepted stream contract.
- I/O failure, closed-handle use, pressure escalation, timeout, and driver failure are typed effects or failures, not normal EOF.

Example:

```styio
prices >> #(p) & risk >> #(r) => {
    p + r -> @signal
}
```

The body runs only after both `p` and `r` are available for the same zip frame.

### Snapshot Join

Snapshot joins are not zip.

Accepted decision:

- A snapshot join reads the latest committed snapshot available at the frame boundary.
- It does not wait for a same-frame partner item.
- It may intentionally read older data.
- The syntax and diagnostics must keep snapshot joins separate from zip joins so users can tell whether they requested barrier synchronization or latest-committed observation.

This preserves the useful distinction between synchronized pairing and low-latency latest-value reads.

## Unequal Rates And Liveness

Accepted decision:

- Fast streams do not silently overwrite or discard pending zip inputs by default.
- Slow streams may create pressure on faster peers or downstream queues.
- A resource or stream family must declare its bounded-buffer, timeout, close, EOF, and pressure-escalation behavior.
- Waiting is not failure by itself.
- A stream that cannot make progress because an input closed, timed out, or failed must settle through the typed resource-effect path.
- Accepted stream programs must be explainable by trace facts: waiting, committed, ended, pressured, escalated, failed, or conflicted.

The language should not allow a "program is doing nothing but not failing" state without a resource-family explanation.

## Backpressure Scheduling

IM-D4 defines backpressure as a typed resource pressure effect. IM-D5 defines where that pressure sits in stream scheduling.

Accepted decision:

- Backpressure is first a non-failure scheduling and resource-pressure signal.
- A pressured stream operation may wait, remain pending, throttle a producer, or trigger a pressure observer according to the resource-family policy.
- Pressure may escalate to `ResourceBackpressureFailure` on timeout, closed channel, failed transport, exceeded backlog, or another declared unrecoverable condition.
- Pressure observers are explicit Styio stream/resource code, such as `channel.pressure >> #(p) => { ... }`.
- `?| operation | backpressure => handler` may run explicit side-effecting recovery logic at the settlement site when the resource family exposes that pressure effect.
- `?| operation | ...` remains a statement-only audited discard. It discards business recovery for that site, but it does not skip scheduling, accounting, diagnostics, cleanup, commit, or pressure escalation.
- The runtime must not use unbounded silent queues as the default answer to pressure.

Backpressure is useful because pressure often means "wait here" rather than "the program failed." The compiler and runtime should preserve that distinction.

## Happens-Before Contract

Accepted decision:

- A frame entry happens after its triggering source item or synchronization barrier is satisfied.
- Reads in the frame happen after frame snapshot capture.
- Writes in the frame happen before that frame's commit boundary.
- A committed resource state is visible to later frames that start after the commit.
- Chained block stages observe the commit from the immediately previous stage.
- Task forms such as `||>` do not create implicit shared mutable access to resources.
- Task results, task failures, and task resource effects cross stream/frame boundaries only through explicit task handles, awaits, resource effects, or commit boundaries.
- Runtime scheduling order is not an accepted source of language-visible ordering.

If the implementation cannot prove a deterministic happens-before relation for an accepted construct, the construct must be rejected by a stable diagnostic or forced through an explicit synchronization/resource effect.

## Parallel Resource Worktree Commit Model

Styio does not allow multiple writers to mutate the same resource subject through shared in-place access. It does allow a Git-like parallel worktree model.

Accepted decision:

- Multiple writers may fork isolated writer snapshots from the same committed resource snapshot.
- Each writer produces its own delta, pending effects, or worktree state.
- At the commit boundary, the resource system attempts a deterministic merge.
- If the merge has one unique, schedule-independent result, the merged state becomes the next committed resource state.
- If the merge is not provably unique, the commit produces a typed `ResourceMergeConflict` effect.
- The runtime must not resolve a conflict by whichever writer happened to finish first.

Merge examples:

| Parallel writer shape | Default behavior |
|-----------------------|------------------|
| Writers update disjoint keys or disjoint structural regions | Deterministic merge may be accepted |
| Writers append to an ordered log or queue | Accepted only if the resource family defines a stable order |
| Writers update the same scalar slot | `ResourceMergeConflict` by default |
| Writers perform commutative updates such as declared counters | Accepted only when the resource family declares a commutative merge rule |
| Writers call native/extern code with host side effects | Not automatically mergeable; must be effect-classified |

This is a language feature, not a scheduler accident. It keeps parallel stream work useful while preserving Styio's deterministic reasoning model.

## Native And Host Side Effects

Native calls can break deterministic stream reasoning if they read global state, mutate files, start threads, or perform hidden I/O.

Accepted decision:

- Code inside a deterministic pulse frame may call native/extern functions only according to their declared effect classification.
- Pure native calls may participate in deterministic frame evaluation.
- Resource-effecting native calls must be represented as resource effects.
- Host-unsafe native calls cannot be assumed frame-deterministic.
- ABI details, manifests, target triples, symbol mapping, headers, and toolchain contracts remain IM-D7.

IM-D5 owns only the stream/concurrency consequence: hidden host effects must not bypass frame, snapshot, commit, pressure, or merge reasoning.

## StyioIR And Runtime Evidence

The IM-D1 verifier owns enforcement, but IM-D5 defines the facts that must survive lowering.

Accepted decision:

StyioIR and runtime traces must be able to represent or recover:

- pulse frame identity,
- triggering source,
- zip barrier membership,
- snapshot-join reads,
- frame-locked resource reads,
- pending writer deltas,
- commit boundaries,
- deterministic merge boundaries,
- `ResourceMergeConflict`,
- pressure state and escalation,
- EOF versus failure, and
- explicit happens-before edges.

Codegen must not guess these facts from source text after lowering.

## Risks And Required Mitigations

| Risk | Mitigation |
|------|------------|
| Barrier zip turns slow streams into a global throttle | Use declared pending queues, pressure effects, timeout, close, and EOF rules; never silently drop by default |
| Pulse snapshots increase memory/runtime cost | Allow resource-family snapshot strategies such as copy-on-write or compact delta tracking, while preserving observable frame consistency |
| Snapshot joins read stale values | Keep snapshot join syntax and diagnostics distinct from zip barrier sync |
| Long waits become invisible liveness bugs | Require traceable wait/pressure/timeout/EOF/failure states |
| Parallel writers conflict | Use deterministic worktree merge and typed `ResourceMergeConflict` instead of scheduler order |
| Native calls break determinism | Require effect classification before treating native calls as deterministic frame operations |
| Optimization moves effects across frame boundaries | Preserve frame, snapshot, commit, sync, and merge boundaries as StyioIR/verifier-visible facts |

## Stop Condition

IM-D5 can close only when:

1. every accepted stream source/consumer combination has parser, sema, lowering, runtime, and fixture coverage;
2. every unsupported stream combination is rejected by a stable diagnostic before placeholder lowering or runtime guessing;
3. zip barrier and snapshot join are represented as distinct lowering/runtime paths;
4. pulse frame identity, snapshot reads, commit boundaries, and happens-before edges survive into StyioIR or verifier-visible metadata;
5. backpressure scheduling has tests for wait, observer, escalation, fallback, and audited discard behavior;
6. finite EOF, close, timeout, and stream failure behavior are tested separately;
7. parallel writer worktree merge has accepted non-overlap/commutative tests and conflict tests; and
8. native/extern stream-frame calls are either effect-classified or rejected from deterministic frame paths.

Until then, unsupported stream/runtime forms must fail closed with named diagnostics.

## Decision Closure

No IM-D5 language-design decision remains open in this inventory. Remaining work is implementation and tests for the accepted deterministic pulse-frame model, zip barrier sync, snapshot joins, pressure scheduling, happens-before edges, and parallel resource worktree merge.

## Source Documents

- [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md)
- [Styio Language Design](../design/Styio-Language-Design.md)
- [Styio Research Innovations](../design/Styio-Research-Innovations.md)
- [Styio EBNF](../design/Styio-EBNF.md)
- [Styio Symbol Reference](../design/Styio-Symbol-Reference.md)
- [IM-D4 Resource Management Inventory](./IM-D4-RESOURCE-MANAGEMENT-INVENTORY.md)
