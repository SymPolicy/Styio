# Styio Observable Language Contract

**Purpose:** Define the durable language and compiler boundary for observable semantic facts, their ownership, identity, evidence, runtime correlation, and consumer isolation. This document does not own source syntax, implementation sequencing, UI design, or telemetry storage.

**Last updated:** 2026-09-04

**Status:** Active design contract; external snapshot and event schemas remain incubating until executable fixtures and compatibility tests exist.

**See also:** [Styio Language Design](./Styio-Language-Design.md), [Resource Topology](./Styio-Resource-Topology.md), [current repository state](../rollups/CURRENT-STATE.md), and [next-stage gap ledger](../rollups/NEXT-STAGE-GAP-LEDGER.md).

---

## 1. Contract

Styio treats observability as a language and compiler property, not as a UI-side reconstruction of source text. The durable objective is to make program structure, data flow, effects, ownership, mutation, failure, task causality, and resource pressure available as facts that:

- the compiler can prove or explicitly mark incomplete;
- the runtime can correlate with concrete execution instances;
- tools can query without depending on compiler object layout;
- Vityo can project without inventing semantic relationships; and
- coding agents can compare across edits and explain changes to users.

The existing compiler-owned Resource Topology is the canonical static graph foundation. Styio must evolve that foundation rather than introduce a second visualization graph with overlapping node and edge semantics.

"One graph" is a consumer experience, not a requirement for one mutable in-memory structure. Static semantic facts, resource planning, runtime instances, and user-owned policy have different owners and lifetimes and must remain separate layers joined by explicit identifiers.

## 2. Authority and layer boundaries

| Layer | Owner | Owns | Must not own |
|---|---|---|---|
| Language semantics | Language SSOT and feature SSOTs | Effects, task/await/cancel behavior, resource visibility and commit semantics, ownership, mutation, failure, and backpressure meaning | UI layout, trace storage, runtime sample aggregation |
| Static topology | Compiler semantic analysis | Proven nodes, edges, facts, completeness, source anchors, evidence, and static validation | Runtime occurrence counts or UI presentation state |
| Resource and execution plan | Compiler planner and runtime contract | Resource requirements, placement intent, queue or executor requirements, and selected plan identity | Reinterpretation of source semantics |
| Runtime overlay | Runtime | Task instances, scheduler transitions, waits, queues, cancellation, failures, timing, loss accounting, and aggregates | Module, effect, ownership, or mutation inference |
| Tooling service | Compiler session and IDE services | Immutable snapshot publication, delta, query, capability negotiation, and bounded retention | A second semantic graph inferred from syntax text |
| Consumers | Vityo and agents | Projection, filtering, source navigation, change review, policy requests, and explanations | Canonical semantic edges, stable identity, or automatic relaxation of user policy |

An observability or consumer failure must never change compiler correctness or program execution semantics. The compiler may reject an explicitly requested artifact when its contract cannot be satisfied, but ordinary check, build, and run behavior must not depend on an exporter or UI being available.

## 3. Current compiler foundation

The current `StyioResourceTopology::Graph` is an internal validation graph built from typed AST state.

Its active node vocabulary is:

- `Program`
- `DriverSource`
- `Handle`
- `StreamOp`
- `StateSlot`
- `HiddenLedger`
- `Sink`
- `Task`
- `FailureDomain`
- `Value`

Its active edge vocabulary is:

- `Flow`
- `Intent`
- `Ownership`
- `Borrow`
- `Mutation`
- `Backpressure`
- `Commit`
- `HappensBefore`
- `Failure`
- `Placement`

`Placement` is currently declared but is not produced by the normal topology builder. The `Eof` type state is likewise declared but is not currently emitted by normal builder state mapping. A declared enum value is not an implemented observable fact until a producer, consumer, and acceptance test exist.

The graph currently supports validation, kind counts, cycle detection, and a diagnostic `debug_string()`. It does not yet provide a persistent semantic ID, immutable published snapshot, evidence model, delta, query index, wire schema, or runtime correlation contract.

Current node IDs are build-order indexes and node source links are process-local AST pointers. Both are valid internal implementation details. Neither may be exposed as a persistent identity. `debug_string()` is diagnostic text, may contain source-derived labels, and is not an external protocol or telemetry format.

Semantic analysis now owns one immutable validated topology artifact per successfully analyzed resource-bearing root. Lowering requires and reuses that exact Sema-owned proof; the import-free narrow scalar subset records an explicit no-op result, while failed or replacement analysis leaves no stale consumable artifact. This internal cutover does not change diagnostics, code generation, or ordinary compiler operation. Persistent semantic IDs, public snapshot publication, serialization, runtime hooks, scheduler correlation, and consumer APIs remain deferred boundaries.

## 4. Static observable artifact

The compiler-side artifact must satisfy these rules before it can become a producer for external consumers:

1. It is produced after the relevant typed semantic facts are available.
2. It is immutable for the remainder of that compilation revision.
3. Lowering and diagnostics consume the same validated artifact instead of rebuilding competing copies.
4. Its traversal order is deterministic for equivalent semantic input.
5. Internal source pointers never cross an artifact or process boundary.
6. Consumer-facing labels and source anchors are explicitly redacted or normalized where required.
7. Partial artifacts state their completeness; recovered facts never appear equivalent to proven facts.
8. Wire serialization is an adapter over the internal artifact, not the owner of compiler data structures.

The artifact may begin as an internal-only capability. An internal capability does not imply an installed SDK, a stable ABI, or a stable wire format.

## 5. Identity and lineage

Styio has three distinct identity classes:

- **Snapshot-local node ID:** an efficient index used only within one immutable artifact.
- **Persistent semantic ID:** an opaque compiler-owned identity for comparing the same semantic site across revisions.
- **Runtime instance ID:** an execution-scoped identity for one concrete task, wait, stream, or resource instance.

A persistent semantic ID must not be derived from:

- line or column numbers;
- byte offsets;
- AST, IR, LLVM, or machine addresses;
- allocation order alone;
- runtime instance identity; or
- a UI-generated random value.

Its logical inputs may include project identity, logical module identity, declaration identity, semantic role, and a compiler-maintained local structural identity. Module identity must use repository or package semantics rather than an absolute machine path. Consumers treat the encoded ID as opaque.

Whitespace, comments, formatting, optimization level, and unrelated local edits must not cause semantic identity churn. Rename, move, rewrite, split, and merge may change identity, but a compiler-known transformation must emit explicit lineage rather than asking consumers to guess from text similarity.

Runtime correlation uses an immutable `snapshot_id` plus a persistent static `site_id`. Runtime instance IDs never replace static site identity.

## 6. Evidence and explanations

Every externally visible semantic edge, derived fact, diagnostic relation, or policy violation must be traceable to structured evidence. Evidence minimally identifies:

- the producing rule and rule version;
- zero or more normalized source anchors;
- prerequisite evidence or semantic subjects;
- completeness;
- privacy-safe attributes needed to explain the conclusion.

Evidence forms a derivation DAG. Human-readable messages are projections of that DAG, not the canonical proof record. Compiler diagnostics, topology explanations, and policy results should share this infrastructure so that three independent explanation systems do not drift.

Early query capabilities should cover node and edge lookup, dependencies, dependents, effects, ownership, mutation, failure, task and stream scope, impact, and `why`. Counterfactual `why-not` analysis is not an initial contract.

## 7. Snapshot, delta, and query boundary

Published snapshots are immutable and explicitly versioned. A snapshot identifies its parent when known, its source revision, compiler build, completeness, and supported capabilities. The mutable caches used to construct or query it remain internal.

Semantic delta is a producer responsibility. A consumer must not be required to heuristically diff two large serialized graphs. Delta distinguishes added, removed, and field-changed nodes, edges, facts, diagnostics, and lineage, and must be replayable against its declared parent snapshot.

The query service may share workspace, file watching, source mapping, and process lifecycle with existing IDE services. Its graph and query contract is independently versioned and must not be hidden inside unspecified LSP extension fields.

## 8. Static and runtime separation

Static topology answers what may or must exist. A runtime overlay answers what occurred in one execution. Neither substitutes for the other.

Runtime events that correlate to topology must carry explicit causal identifiers when available. Timestamp proximity is not evidence that one task woke, blocked, cancelled, or backpressured another. Wait observations must distinguish at least runnable scheduling latency, cooperative suspension, I/O, resource, task, backpressure, timer, cancellation, and unknown waits before a consumer presents a precise cause.

High-frequency observations default to aggregation or sampling. Lifecycle, failure, cancellation, and loss-accounting events have higher retention value than per-item stream samples. Telemetry buffers must not block the application merely to preserve low-priority observations.

OpenTelemetry and Perfetto are exporter targets. They do not define Styio's static ownership, mutation, effect, type, failure, or resource semantics.

## 9. Privacy and cost

Observable artifacts and events are private-by-default:

- do not emit raw values, source text, credentials, filesystem roots, environment contents, or backend runtime records by default;
- prefer type, shape, count, duration, size, capability, and privacy-safe summaries;
- require explicit capability and redaction policy before exposing a raw value;
- account for dropped telemetry and incomplete observation; and
- measure wall time, CPU, allocation, memory, artifact size, and event volume before enabling a mode by default.

The disabled or internal-only path must not impose exporter dependencies on ordinary compiler operation. New indexes and caches must have an identified owner and bounded lifetime.

## 10. Versioning and migration

Language edition, compiler implementation version, topology snapshot schema, runtime event schema, and IDE or debugger protocol versions are independent. A change in one does not silently redefine the others.

External schemas begin in an explicitly incubating state. They become stable only after producer, consumer fixture, compatibility behavior, and regression evidence exist. Unknown additive fields may be ignored or preserved; unknown critical capabilities must be rejected with a machine-readable reason.

An implementation migration must state the current behavior, target behavior, cutover condition, owning SSOTs, and acceptance evidence. The default is one complete cutover of implementation, documentation, and tests. Parallel old and new implementations or compatibility layers require an explicit language compatibility contract; otherwise exact historical behavior is recovered from Git history.

Artifact and source filenames use semantic names rather than `v2`, `new`, `old`, `legacy`, or `latest` generations.

## 11. Non-goals

This contract does not:

- define new Styio source syntax or a source-level trace operator;
- redefine resource visibility, pending writes, commit barriers, or active effect rows;
- promise debugger UI, breakpoints, stepping, DAP, or deterministic replay;
- define a telemetry backend, log store, profiler database, or audit retention system;
- make arbitrary compiler internals public or stable;
- make runtime samples the canonical project model;
- allow Vityo to infer canonical semantic edges from text, call stacks, or addresses;
- connect compiler topology directly to the runtime scheduler before a versioned correlation contract exists; or
- allow an agent to weaken user-owned hard policy to make a change pass.

## 12. Evidence obligations

Current compiler behavior remains guarded by `styio_resource_topology_test` and the resource, task, state, stream, Sema, lowering, and IDE suites registered in `tests/CMakeLists.txt` and [the test catalog](../../workflows/TEST-CATALOG.md). Focused lifecycle evidence proves move-only const observation, successful publication, scalar no-op, failure/reanalysis cleanup, same-artifact lowering reuse, mismatched-root rejection, and compiler-owned IDE diagnostics; a source oracle proves lowering contains no alternate topology builder or validator path.

Each delivered observable capability must add the narrowest applicable evidence:

- deterministic static golden output;
- semantic-identity metamorphic tests for formatting, comments, and unrelated edits;
- delta replay properties;
- evidence graph resolution;
- partial-program behavior;
- static-site to runtime-instance correlation;
- telemetry loss behavior;
- bounded overhead measurements; and
- consumer fixtures that do not link compiler internals.

Implementation sequencing, ownership, and current acceptance state belong in the tracked Better Plan workspace and active gap ledger, not in this design SSOT.
