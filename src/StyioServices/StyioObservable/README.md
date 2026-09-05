# StyioObservable

**Purpose:** Own the incubating public observable topology contracts: schema-v1 static snapshots, producer-owned snapshot delta and lineage, bounded query evaluation, and per-scope retention. This README plus `tests/fixtures/observable-topology/` and `tests/fixtures/observable_static_snapshot/v1/` is the decoder SSOT.

**Last updated:** 2026-09-05

**Status:** Incubating. Snapshot schema v1 and S2 delta, lineage, and bounded query contracts at `0.1` are implemented. Runtime-events schema v2 is implemented behind an explicit compile-plan request and remains unapproved for default enablement. Private editor transports and Vityo UI remain out of scope.

The long-term vocabulary in `docs/design/Styio-Observable-Language.md` does not override this wire schema.

## Files

Public contract library `styio_observable_core` (no compiler-internal or third-party JSON implementation libraries):

| File | Owns |
|------|------|
| `Snapshot.hpp/.cpp` | Schema-v1 snapshot value model, canonical compact serializer, `finalize_snapshot` / `parse_snapshot`, snapshot identity `s1_` plus 32 hex. |
| `JsonSupport.hpp` | Self-contained compact JSON writer, parser, and SHA-256 helper used only by the public serializer. |
| `Delta.hpp/.cpp` | Incubating delta envelope, linear-merge generation, transactional apply, lineage construction. |
| `Query.hpp/.cpp` | Bounded query request/response, negotiation, reference evaluator, immutable index shards. |
| `RuntimeCorrelation.hpp/.cpp` | Incubating runtime-events schema v2: identifiers, event/wait vocabularies, sampling, conservation, privacy canaries, and the strict serializer/parser. No compiler, runtime, or LLVM includes. |

Compiler-side producer adapters that translate a validated topology artifact into the public snapshot (`StaticSnapshotContract`, `StaticSnapshotPublication`) and the delta emission stage (`DeltaPublication`) live in the sibling `StyioObservableProducer/` directory and are linked only into the full compiler. They are not part of `styio_observable_core`.

## Snapshot request (compiler)

Full Styio consumes an optional compile-plan emission object. Nano does not parse or publish snapshots.

```json
{
  "emit": {
    "observable_static_snapshot": {
      "schema_version": 1,
      "required_capabilities": [
        "file-source-anchors",
        "producer-evidence",
        "static-topology-edges",
        "static-topology-facts",
        "static-topology-nodes"
      ],
      "parent_snapshot_path": "/abs/path/previous/<output-stem>.observable-static-snapshot.json"
    }
  }
}
```

Rules:

1. The object is optional. If absent, the compiler allocates no snapshot model, JSON buffer, or profiler phase.
2. Only `schema_version`, `required_capabilities`, and the optional `parent_snapshot_path` are allowed. Any other field is `observable_static_snapshot_malformed`.
3. `schema_version` must be the integer `1`.
4. `required_capabilities` is an array of unique non-empty strings drawn from the closed set below. An empty array is admitted and still publishes the full closed set. The S2 optional capabilities (`snapshot-delta`, `producer-lineage`, `bounded-query`) are negotiated independently and are `observable_static_snapshot_unsupported_capability` when listed here.
5. Admission requires `generated_by.tool = "pafio"`, exactly one package record matching `entry.package_id`, a namespaced package `name`, absolute package `root` and `manifest`, and canonical package-relative slash paths for the manifest and entry file.
6. Direct-file `generated_by.tool = "styio"` is `observable_static_snapshot_direct_file`. Any other producer is `observable_static_snapshot_styio_produced`.
7. `parent_snapshot_path`, when present, must be a non-empty string naming the previous snapshot artifact of the same compilation unit (Pafio writes the absolute, lexically normalized path it received from `pafio check --emit-observable-static-snapshot --observable-parent-snapshot <path>`). It is a transport input only: it never participates in admission, snapshot identity, or snapshot bytes, and it is read only after the snapshot has been published. An empty string or a non-string value is `observable_static_snapshot_malformed`. When the field is absent, no delta stage runs, no parent file is read, and no delta material is allocated.

## Snapshot output

Successful publication writes compact JSON with one trailing newline to:

`<artifact-dir>/<output-stem>.observable-static-snapshot.json`

The path is appended to `receipt.json` `artifacts`. A publication or write failure emits no partial snapshot file and uses diagnostic subcode `observable_static_snapshot` or a more specific `observable_static_snapshot_*` admission subcode.

## Delta publication (compiler)

When the admitted request carries `parent_snapshot_path`, the full compiler runs the delta stage immediately after the snapshot artifact has been written, inside the same `observable_static_snapshot` profiler phase. The stage is producer-owned and uses only the public S2 library semantics (`parse_snapshot`, `generate_delta`, `apply_delta`); it adds no heuristics.

Stage order, each step failing closed to the listed `reason`:

| Step | Failure `reason` |
|------|------------------|
| Read the parent file as bytes (must be a regular, readable file) | `parent_unreadable` |
| Decode it with `parse_snapshot`; the bytes must already be the canonical serialization of the decoded snapshot (otherwise the identity of the bytes on disk would differ from the identity the delta names) | `parent_invalid` |
| Require the same `contract`, `schema_version`, and `compilation_unit` (`package_name`, `manifest_path`, `entry_path`) as the snapshot just published | `parent_mismatch` |
| `generate_delta(parent, current)`; the result must name the parent and current identities and `apply_delta(parent, delta)` must reconstruct the published bytes exactly | `delta_failed` |
| Write the canonical delta JSON through the compile-plan artifact writer | `write_failed` |

The `reason` set is closed: `parent_unreadable`, `parent_invalid`, `parent_mismatch`, `delta_failed`, `write_failed`.

Successful publication writes compact JSON plus one trailing newline (the canonical `serialize_delta` output of the envelope below) to:

`<artifact-dir>/<output-stem>.observable-delta.json`

and appends that path to `receipt.json` `artifacts` after the snapshot path. `parent_snapshot_id` is the identity of the parent bytes; `target_snapshot_id` is the identity of the snapshot artifact written in the same run. `required_capabilities` is the published snapshot capability list; `optional_capabilities` is `["snapshot-delta"]`, plus `producer-lineage` only when the current snapshot carries producer `lineage` records (the compiler publishes none today, so no `lineage` category operations are emitted). A write failure removes any partial delta file.

The delta stage never fails compilation and never fails the snapshot publication: the snapshot artifact, its `artifacts` entry, and the compiler exit code are identical whether the delta stage succeeds or degrades. No diagnostic is emitted for a degraded delta; the receipt is the only signal.

### Receipt record

`receipt.json` gains the key `observable_static_snapshot` (placed after `artifacts`) only when `parent_snapshot_path` was present in the request. Without the field the receipt is byte-identical to the S1 shape. Exactly one of two shapes appears:

```json
"observable_static_snapshot": {
  "delta": "published",
  "parent_snapshot_id": "s1_<32 hex>",
  "target_snapshot_id": "s1_<32 hex>"
}
```

```json
"observable_static_snapshot": {
  "delta": "full_snapshot_required",
  "reason": "parent_unreadable"
}
```

A consumer that holds the parent snapshot applies the delta with `apply_delta(parent, delta)` and must obtain `target_snapshot_id`; on `full_snapshot_required` it reads the freshly written snapshot artifact instead. The parent path itself never appears in the snapshot, the delta, or the receipt; the receipt lists only the current run's artifact paths under `artifacts`, exactly as before.

Profiler counters added to the `observable_static_snapshot` phase when the stage runs: `delta_operation_count` and `delta_serialized_bytes` (both `0` on degradation).

## Snapshot top-level key order

Every schema-v1 snapshot emits these keys in this exact order:

1. `contract`
2. `schema_version`
3. `stability`
4. `producer`
5. `capabilities`
6. `compilation_unit`
7. `completeness`
8. `root`
9. `nodes`
10. `edges`
11. `facts`
12. `anchors`
13. `evidence`

`contract` is always `styio.observable.static-snapshot`. `schema_version` is always `1`. `stability` is always `incubating`.

`producer` is `{ "name": "styio", "version": "<STYIO_PROJECT_VERSION>" }`.

`compilation_unit` is `{ "package_name", "manifest_path", "entry_path" }` using logical package identity and package-relative slash paths. Absolute workspace or package roots never appear.

Collections are sorted by their identity field (`id` or `ref`) and contain unique identities. Compatible additive fields may appear after `evidence`. Schema-v1 consumers ignore unknown fields. S2 additive keys, omitted when empty so schema-v1 goldens stay byte-identical, are:

- `diagnostics` — optional array of `{id,code,severity,subject,evidence}`
- `lineage` — optional array of lineage records (see below)
- `parent_snapshot_id` — optional `s1_` identity of the retained parent snapshot

Unknown critical behavior named by completeness, schema version, or a missing required capability must fail closed.

Snapshot identity is `s1_` plus the first 32 hex characters of SHA-256 over the canonical snapshot JSON including the trailing newline.

## Snapshot capabilities

The closed, sorted advertised set is:

- `file-source-anchors`
- `producer-evidence`
- `static-topology-edges`
- `static-topology-facts`
- `static-topology-nodes`

S2 optional capabilities, independently negotiated, are:

- `snapshot-delta`
- `producer-lineage`
- `bounded-query`

Full `--machine-info=json` reports:

```json
"observable_static_snapshot": {
  "schema_versions": [1],
  "capabilities": ["file-source-anchors", "producer-evidence", "static-topology-edges", "static-topology-facts", "static-topology-nodes"],
  "optional_capabilities": ["producer-lineage", "snapshot-delta"]
},
"observable_delta": { "schema_versions": [{ "major": 0, "minor": 1 }] }
```

`capabilities` is the closed set admissible as `required_capabilities`. `optional_capabilities` advertises the independently negotiated S2 capabilities the producer can emit through the compile plan (`snapshot-delta` via `parent_snapshot_path`; `producer-lineage` names the lineage contract carried inside that delta) and is never admissible as required. `observable_delta.schema_versions` lists the delta envelope versions the producer writes. Nano reports `{"schema_versions":[],"capabilities":[],"optional_capabilities":[]}` and `{"schema_versions":[]}`.

## Completeness

| Value | When | Shape |
|-------|------|-------|
| `complete/validated-topology` | Sema published a validated topology artifact | Program-rooted nodes, edges, facts, file anchors, and evidence |
| `complete/proven-scalar-noop` | Sema recorded the scalar no-op lifecycle | `root` is JSON `null`; `nodes`, `edges`, `facts`, `anchors`, and `evidence` are empty arrays; no graph is constructed |

Any other completeness string is unsupported. Negative query answers are conclusive only for `complete/validated-topology`. Incomplete snapshots return `partial` rather than proven absence.

## Identifiers

Public IDs are versioned prefixes plus 32 lowercase hex characters (first 128 bits of SHA-256 over a length-prefixed preimage):

| Kind | Prefix | Domain |
|------|--------|--------|
| node | `n1_` | qualified semantic identity (`styio.semantic-resource-node.v2`) |
| edge | `e1_` | compilation unit + edge kind + endpoint public IDs + relation key |
| fact | `f1_` | compilation unit + subject node ID + predicate (not value) |
| anchor | `a1_` | compilation unit + entry-relative path |
| evidence | `v1_` | compilation unit + producer rule + version + subjects + prerequisites + anchors |
| snapshot | `s1_` | SHA-256 of canonical snapshot JSON |
| lineage | `l1_` | kind + producer rule + version + prior + target + evidence |
| diagnostic | `d1_` | producer-assigned diagnostic identity |

Dense graph indexes, pointers, labels, absolute paths, raw source, content hashes, and compiler object addresses are not public IDs.

## Nodes, edges, and facts

Node `kind` spellings currently produced:

`Program`, `DriverSource`, `Handle`, `StreamOp`, `StateSlot`, `HiddenLedger`, `Sink`, `Task`, `FailureDomain`, `Value`

Node `role` spellings currently produced:

`Program`, `StandaloneBlock`, `DriverSource`, `ResourceSlot`, `ResourceHandle`, `ResourceMethod`, `StreamOperation`, `StateSlot`, `Snapshot`, `Value`, `Sink`, `Task`, `StateWindowLedger`, `SeriesLedger`, `TaskFailureDomain`, `ScopeExitDestroySink`

Edge `kind` spellings currently produced:

`Flow`, `Intent`, `Ownership`, `Borrow`, `Mutation`, `Backpressure`, `Commit`, `HappensBefore`, `Failure`

`Placement` is declared on the internal graph enum and is not produced by the normal builder. Consumers must not require it.

Each node has two facts:

- predicate `capabilities` — sorted capability-name array (`pull`, `iter`, `push`, `close`, `clone`, `checkpoint`, `task`, `state-read`, `state-write`)
- predicate `type-state` — `Unknown`, `Declared`, `Open`, `Eof`, `Closed`, `Materialized`, or `Ready`

`Eof` is declared and is not currently emitted by normal builder mapping.

## Anchors and evidence

Schema v1 uses file precision only. Source-owned nodes share one anchor whose `path` is the compilation-unit entry-relative path and whose `precision` is `file`. Synthetic nodes omit anchors. Line and column are not published.

Each evidence record has `ref`, `producer_rule`, `rule_version` (`"1"`), sorted unique `subjects`, `prerequisites`, and `anchors`.

- Node evidence: `styio.sema.topology.node.<Role>`
- Edge evidence: `styio.sema.topology.relation.<Kind>.<relation-key-or-default>` with both endpoint node-evidence records as prerequisites
- Fact evidence: `styio.observable.static.fact.normalize.<predicate>` with the subject node evidence as prerequisite

Relation keys are a closed compiler-owned vocabulary. Source identifiers never appear in `producer_rule` or anywhere else in the snapshot.

The evidence graph is a DAG. Unresolved references, duplicate identities, incomplete descriptors, and cycles fail closed and emit no JSON.

## Delta envelope

Contract `styio.observable.delta`, incubating schema `{ "major": 0, "minor": 1 }`. Compact JSON plus one trailing newline.

```json
{
  "contract": "styio.observable.delta",
  "schema_version": { "major": 0, "minor": 1 },
  "stability": "incubating",
  "parent_snapshot_id": "s1_<32 hex>",
  "target_snapshot_id": "s1_<32 hex>",
  "required_capabilities": ["static-topology-nodes"],
  "optional_capabilities": ["snapshot-delta"],
  "operations": []
}
```

`generate_delta(A, B)` is a linear merge over each canonical category. `apply_delta(A, delta)` reconstructs canonical `B` or fails before any caller-visible mutation. Operation order is category (`metadata`, `nodes`, `edges`, `facts`, `diagnostics`, `anchors`, `evidence`, `lineage`), then record key, then field name.

| `op` | Meaning |
|------|---------|
| `add` | Insert the `record` object under `key` |
| `remove` | Delete the record identified by `key` |
| `replace_fields` | Replace named non-key fields; each field has `name`, `before`, and `after` as raw JSON fragments |

Operations must be unique per (`category`, `key`) and sorted by category then key; `apply_delta` rejects unsorted or duplicated operations as `malformed_operation`. Identity-field changes are remove plus add, never silent mutation: `apply_delta` rejects a `replace_fields` operation that names an identity field. The replaceable (non-key) fields per category are:

| Category | Replaceable fields | Identity fields (remove plus add only) |
|----------|--------------------|----------------------------------------|
| `metadata` | `completeness`, `producer_name`, `producer_version`, `root`, `capabilities`, `package_name`, `manifest_path`, `entry_path`, `parent_snapshot_id` | — |
| `nodes` | `kind`, `role`, `anchors`, `evidence` | `id` (the persistent site ID) |
| `edges` | `evidence` | `kind`, `from`, `to` |
| `facts` | `value`, `evidence` | `subject`, `predicate` |
| `diagnostics` | `code`, `severity`, `evidence` | `subject` |
| `anchors` | `precision` | `path` |
| `evidence` | — | `producer_rule`, `rule_version`, `subjects`, `prerequisites`, `anchors` |
| `lineage` | `completeness` | `kind`, `prior`, `target`, `producer_rule`, `rule_version`, `evidence` |

Metadata uses key `"snapshot"` and `replace_fields` only. `parent_snapshot_id` is the JSON string `s1_<32 hex>` of the parent snapshot, or JSON `null` when the snapshot names no parent; the canonical serializer omits the key when empty.

Apply fail-closed reasons:

| `reason` | When |
|----------|------|
| `wrong_base` | `parent_snapshot_id` does not match the supplied snapshot |
| `malformed_operation` | Unknown op/category, missing record, before-value mismatch, missing key, identity-field replacement, or unsorted/duplicated operations |
| `unknown_required_capability` | A required capability is outside the closed sets |
| `unsupported` | Delta schema major is not `0` |
| `invalid` | Applied bytes do not equal the declared `target_snapshot_id` |

## Lineage

Lineage is published only when the compiler-side producer supplies structured drafts. Consumers may display or query records; they must not promote names, positions, timestamps, or hints into canonical relations. Absent evidence means unknown.

Canonical producer rules start with `styio.sema.` or `styio.observable.`.

| `kind` | Cardinality |
|--------|-------------|
| `rename` | prior 1, target 1 |
| `move` | prior 1, target 1 |
| `split` | prior 1, target N≥2 |
| `merge` | prior N≥2, target 1 |

Every prior subject must exist in parent A, every target subject in child B, and every evidence ref must resolve. Same-ID continuity requires no lineage record.

Record shape:

```json
{
  "id": "l1_<32 hex>",
  "kind": "rename",
  "prior": ["n1_<32 hex>"],
  "target": ["n1_<32 hex>"],
  "producer_rule": "styio.sema.topology.lineage.rename",
  "rule_version": "1",
  "evidence": ["v1_<32 hex>"],
  "completeness": "complete"
}
```

## Bounded query

Contract `styio.observable.query`, incubating schema `0.1`. Every request declares finite limits. Hard ceilings clamp silently; they do not yield `invalid`.

Defaults: `max_results` 256, `max_depth` 16, `max_visited` 4096, `max_evidence` 1024.

Hard ceilings: 4096, 64, 65536, 8192.

Kinds: `lookup`, `dependencies`, `dependents`, `effects`, `ownership`, `mutation`, `failure`, `task_scope`, `stream_scope`, `impact`, `canonical_path`, `lineage`, `why`.

Request:

```json
{
  "contract": "styio.observable.query",
  "kind": "canonical_path",
  "subject": "n1_<32 hex>",
  "target": "n1_<32 hex>",
  "limits": {
    "max_results": 256,
    "max_depth": 16,
    "max_visited": 4096,
    "max_evidence": 1024
  },
  "required_capabilities": ["bounded-query"],
  "optional_capabilities": []
}
```

`canonical_path` is bounded BFS over `Flow` edges (or caller `relation_kinds`) with lexicographic neighbor-id tie-break. Result order is the path order and is not re-sorted by id.

`why` is a bounded walk of the published evidence DAG from the subject's evidence record.

Response envelope:

```json
{
  "snapshot_id": "s1_<32 hex>",
  "status": "complete",
  "completeness": "complete/validated-topology",
  "reason": "",
  "results": [{"category": "nodes", "id": "n1_<32 hex>", "record": {}}],
  "evidence": ["v1_<32 hex>"],
  "visited": 3,
  "evidence_count": 1,
  "truncated": false
}
```

| `status` | Meaning |
|----------|---------|
| `complete` | Conclusive for this snapshot completeness |
| `partial` | Missing subject on a non-conclusive snapshot |
| `truncated` | A result, depth, visited, or evidence ceiling stopped the walk |
| `unsupported` | Unknown required capability or incompatible query major |
| `invalid` | Malformed request or no retained snapshot |

`visited` counts the candidate records the evaluation examines: the subject lookup, every incident edge considered, every BFS expansion, and every lineage or evidence record admitted into the walk. It never exceeds `max_visited`.

The service answers from immutable per-subject index shards. A shard stores the subject's sorted incident edge IDs, fact IDs, and lineage IDs plus the dependency keys that invalidate it; record payloads are resolved from the retained canonical snapshot, never duplicated in the shard. The full-snapshot reference evaluator remains the semantic oracle and the bounded fallback when derived indexes are absent. Indexed and reference answers are identical canonical facts, evidence, and visited counts. Cache loss degrades to the reference evaluator; it never changes canonical facts.

## Negotiation

Snapshot schema, delta schema, lineage capability, and query protocol are independently versioned. Negotiation requires equal major, selects the highest common minor, intersects optional capabilities, and rejects unknown required capabilities. A compile-plan consumer negotiates from `--machine-info=json`: `observable_static_snapshot.schema_versions` for the snapshot request, `observable_delta.schema_versions` for the delta envelope, and `observable_static_snapshot.optional_capabilities` for `snapshot-delta` / `producer-lineage`.

Stable reasons: `major_incompatible`, `unknown_required_capability`, `unsupported`.

## Service, retention, and degradation

`ObservableTopologyService` binds one qualified scope (`package_name`, `manifest_path`, `entry_path`). It retains at most two snapshots and one parent-to-current delta. Defaults: 64 MiB snapshot bytes and 64 MiB derived-index bytes. It caches immutable index shards, not final query answers.

Publication is atomic: validate, generate/apply delta, merge index shards, check byte limits, then swap. Invalid input leaves prior state unchanged. A child index merge reuses a parent shard only when the shard's subject sits outside the invalidation closure of the delta seed set — the changed record keys plus the endpoints of added edges, facts, and lineage — so unrelated shards are shared unchanged and only dependency-reached shards are rebuilt from the child.

Degradation:

| Situation | Outcome |
|-----------|---------|
| No retained parent / evicted history | `full_snapshot_required` |
| Dropped derived indexes | Bounded reference evaluation |
| Incomplete snapshot | `partial` non-conclusive answers |
| Exhausted query budget | `truncated` canonical prefix |
| Oversized publication | `resource_limit`; prior state preserved |

Path-free counters: `input_records`, `changed_records`, `delta_bytes`, `visited_records`, `reused_shards`, `rebuilt_shards`, `retained_snapshot_bytes`, `retained_index_bytes`, `reference_fallbacks`. Wall time, RSS, and thresholds belong to `styio-benchmark`, not this tree.

## Fixtures and consumer isolation

Checked-in corpus: `tests/fixtures/observable-topology/` (`manifest.json` plus parent/child/delta/lineage/query families covering unchanged, add, remove, field-change, rename, move, split, merge, partial, malformed, and unsupported). Schema-v1 snapshot goldens remain at `tests/fixtures/observable_static_snapshot/v1/`. Runtime-events v2 producer/consumer fixtures live at `tests/fixtures/observable-runtime-correlation/v2/`.

`tests/observable_topology_consumer_test.cpp` links only `styio_observable_core`. It negotiates, deserializes, applies deltas, and queries without compiler, editor, runtime, or LLVM implementation libraries. Runtime-event consumers parse the public v2 serializer output the same way: map fields, do not redefine site identity, causality, waits, or loss.

An independent decoder can be written from this README and those fixtures alone.

## Runtime-events v2

Contract `styio.observable.runtime-events`, schema version `2`, stability `incubating`, privacy profile `strict`. Styio is the sole semantic owner. Pafio may request and route `build_root/runtime-events.jsonl`; Vityo, agents, OpenTelemetry, and Perfetto adapters may map fields. None may invent site identity, causal edges, wait classification, completeness, or loss.

`--machine-info=json` advertises version `2` only. Nano advertises none. Version 1, unknown versions, and unsupported required capabilities fail before execution with stable subcodes (`runtime_events_unsupported_version`, `runtime_events_unknown_version`, `runtime_observation_unsupported_capability`). There is no Adapter and no compatibility serializer.

### Compile-plan request

```json
{
  "emit": {
    "runtime_observation": {
      "version": 2,
      "mode": "aggregate",
      "required_capabilities": ["task-lifecycle", "loss-accounting"]
    }
  }
}
```

Rules:

1. The object is optional. If absent, compile-plan runs still write v2 controller JSONL with mode `disabled` and allocate no S3 runtime buffers, drain owner, or observed ABI.
2. Observation requires an admitted qualified compilation unit (package name, root, manifest), matching static-snapshot admission.
3. `mode` is the closed set `disabled`, `aggregate` (explicit-request default), `sampled` (default 1/16, seed 0), `detailed`.
4. Lane capacity must be a power of two in `[64, 4096]`. Default capacity 256 with 32 priority-reserved slots. These are protocol-visible resource settings; they do not mean loss cannot occur.
5. Cancellation and cooperative suspend/resume kinds exist in the schema and fixtures. Producer capabilities stay false until an accepted runtime transition exists. Tests serialize those kinds without treating elapsed time as cancellation.

### Wire shape

A stream begins with exactly one `session.capability` record and, when the sink remains writable, ends with exactly one `session.summary` record. Every line is one self-contained JSON object (JSONL). `record_kind` is `session.capability`, `event`, or `session.summary`; the two framing records repeat their name as `event_kind`. The compile-plan receipt names the artifact as `outputs.runtime_events_path` in `<build_root>/receipt.json`; the artifact itself is `<build_root>/runtime-events.jsonl`.

Identifiers are opaque strings: a two-character prefix plus sixteen lowercase hex digits packing a 16-bit producer lane over a 48-bit lane-local sequence. `i2_` is an instance, `r2_` an event reference, `w2_` a wait episode, `x2_` the execution. Lane `0` is the controller; lanes `1..producer_lanes` are scheduler workers. The session always covers the scheduler worker count, so every worker owns exactly one single-producer lane.

Correlatable events carry `snapshot_id`, `site_id`, `instance_id`, `event_id`, `event_kind`, and typed `role`. Runtime-only scheduler facts use `correlation_status: "runtime_only"`. Unavailable correlation is never a zero site. Causal `causes` are typed edges; consumers must not derive edges from order or time. Wait records share one `wait_id` across begin/end and name runnable, task, or backpressure producers today. Duration is valid only when both records use the session monotonic clock.

### Vocabulary

`mode`: `disabled`, `aggregate`, `sampled`, `detailed`.

`event_kind` spellings, with the family and priority each maps to:

| `event_kind` | `family` | `priority` |
| --- | --- | --- |
| `session.capability`, `session.summary` | `session` | `lifecycle` |
| `compile.started`, `compile.finished`, `compile.failed`, `unit.entered`, `unit.exited`, `unit.test.started`, `unit.test.finished`, `transition.fired`, `state.changed`, `diagnostic.emitted`, `run.started`, `run.finished`, `thread.spawned`, `thread.exited`, `log.emitted` | `controller` | `lifecycle` |
| `task.created`, `task.enqueued`, `task.started`, `task.completed`, `task.failed`, `task.result_consumed`, `task.released`, `cancellation.requested`, `cancellation.completed`, `cooperative.suspend`, `cooperative.resume` | `task_lifecycle` | `lifecycle` |
| `task.dequeued` | `task_lifecycle` | `detail` |
| `queue.closed` | `queue` | `lifecycle` |
| `queue.pressure` | `queue` | `detail` |
| `wait.begin`, `wait.end` | `wait` | `lifecycle` |
| `aggregate.shard` | `aggregate` | `detail` |

`family` spellings are `session`, `controller`, `task_lifecycle`, `queue`, `wait`, `causal`, `aggregate`, `detail`. The summary always reports all eight rows; `causal` and `detail` currently have no producing event kinds and stay zero.

`priority` is `lifecycle` or `detail`. Only detail records are aggregated in `aggregate` mode or sampled in `sampled` mode; lifecycle records are always retained.

`correlation_status`: `correlated` (descriptor-resolved site), `runtime_only`, `unavailable`.

`role`: `task`, `await`, `runtime_only`.

Causal edge `kind`: `spawn`, `enqueue`, `dispatch`, `completion`, `wake`, `failure`, `cancellation`, `backpressure_relief`. Producers today emit `spawn`, `dispatch`, `completion`, `wake`, and `failure`.

Wait `reason`: `runnable`, `cooperative`, `io`, `resource`, `task`, `backpressure`, `timer`, `cancellation`, `unknown`. Producers today emit `runnable`, `task`, and `backpressure`; the rest are schema-owned and advertised unavailable.

Wait `resolution` (only on `wait.end`; `null` on `wait.begin`): `ready`, `completed`, `failed`, `cancelled`, `timed_out`, `closed`, `unknown`. Producers today emit `ready`, `completed`, `failed`, and `closed`.

### Record fields

`session.capability`: `contract`, `schema_version` (`2`), `stability` (`incubating`), `record_kind`, `event_kind`, `mode`, `snapshot_schema`, `snapshot_id` (`null` when none), `execution_id`, `privacy_profile` (`strict`), `producer_lanes`, `lane_capacity`, `priority_reserved`, `drain_batch`, `sampling` (`numerator`, `denominator`, `seed`), `clock_unit` (`ns`), `supported_capabilities`, `active_capabilities`, `unavailable_capabilities`. Supported/active spellings: `task-lifecycle`, `scheduler-queue`, `wait-runnable`, `wait-task`, `wait-backpressure`, `controller-events`, `loss-accounting`, `strict-privacy`. Unavailable: `cancellation`, `cooperative-suspend`, `wait-cooperative`, `wait-io`, `wait-resource`, `wait-timer`. In `disabled` mode `active_capabilities` is empty.

`event`: `contract`, `schema_version`, `record_kind` (`event`), `event_kind`, `family`, `priority`, `correlation_status`, `role`, `snapshot_id` and `site_id` (strings only when `correlated`, else `null`), `instance_id` (`null` only for zero-instance non-task, non-wait records), `event_id`, `monotonic_ns`, `causes` (array of `kind` / `event_id` / `subject_instance`, possibly empty), `wait` (`null` unless `wait.begin`/`wait.end`; then `wait_id`, `waiter_instance`, `subject_instance` (`null` when absent), `reason`, `resolution`, `duration_ns` (`null` when invalid)). `queue_depth` and `queue_capacity` appear on `queue.pressure`, `queue.closed`, `wait.begin`, and `wait.end`. `count` and `duration_ns` appear on `aggregate.shard`. Controller annotations `unit_id`, `test_name`, `intent`, `phase`, `operation`, `diagnostic_code`, `stream`, `from_phase`, `to_phase`, `final_phase` appear only when non-empty; `success` and `executed` appear only when meaningful.

`session.summary`: `contract`, `schema_version`, `record_kind`, `event_kind`, `mode`, `execution_id`, `completeness`, `exporter_failed`, `lane_capacity`, `priority_reserved`, `producer_lanes`, `high_water_occupancy`, and `families`: an object keyed by every family spelling, each with `observed`, `emitted`, `aggregated`, `sampled_out`, `buffer_dropped`, `exporter_dropped`, and `summary_updates`.

Every family row satisfies the conservation equation `observed = emitted + aggregated + sampled_out + buffer_dropped + exporter_dropped`. `emitted` counts records written to the sink; a record the exporter fails to write moves from `emitted` to `exporter_dropped`. `summary_updates` is a sidecar projection, not a disposition. Session framing records are excluded from the ledger.

`completeness` spellings and meanings, in precedence order (the first matching condition wins): `partial/exporter_failure` (sink failed), `partial/buffer_loss` (`buffer_dropped` nonzero), `partial/export_loss` (`exporter_dropped` nonzero), `partial/unresolved` (a `wait.begin` never gained its `wait.end`, e.g. a worker still parked at session end), `partial/sampling` (sampled mode shed records), `partial/aggregation` (aggregate mode folded records), `complete` (all producers ran, all retained references resolve, no loss, summary written). `partial/disabled` marks a disabled-mode controller-only session.

V2 omits log payload text, source paths, raw diagnostic messages, values, labels, environment, and addresses. Diagnostics may carry a stable `diagnostic_code`. Source navigation resolves `site_id` in the separately authorized S1 snapshot.

Unknown additive v2 fields are ignored by `parse_runtime_record`. Unknown enum values, versions, or capabilities marked critical are rejected. Rejection subcodes: `runtime_events_unsupported_version` (version 1), `runtime_events_unknown_version`, `runtime_observation_malformed`, `runtime_observation_unsupported_capability`, `runtime_observation_unsupported_mode`, `runtime_observation_invalid_bounds`.

S3 remains unapproved for default enablement. Disabled and static-only paths keep the existing task ABI (`styio_task_*_spawn` without `_observed`). Enabled codegen registers a compact descriptor table and calls `styio_task_*_spawn_observed`.

See `tests/fixtures/observable-runtime-correlation/v2/` and `tests/observable_runtime_test.cpp`. Numeric budgets are not encoded here; `benchmark/` owns the measurement seam and rejects missing or unapproved result contracts.

## Privacy

Snapshots, deltas, lineage, queries, runtime-events v2, fixtures, errors, and counters must not contain workspace or package roots, absolute paths, raw source or literal values, content hashes, graph labels, pointer/address text, compiler object IDs, dense graph IDs, `source_text`, `raw_value`, credentials, or backend runtime records. The compile-plan `parent_snapshot_path` is consumed by the producer and never copied into any published artifact or receipt field. Runtime-events v2 also omits `log.emitted` payload text and raw diagnostic messages.

See the service inventory in [../MANIFEST.md](../MANIFEST.md).
