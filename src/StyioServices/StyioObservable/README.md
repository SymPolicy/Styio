# StyioObservable

**Purpose:** Own the incubating public observable topology contracts: schema-v1 static snapshots, producer-owned snapshot delta and lineage, bounded query evaluation, and per-scope retention. This README plus `tests/fixtures/observable-topology/` and `tests/fixtures/observable_static_snapshot/v1/` is the decoder SSOT.

**Last updated:** 2026-09-05

**Status:** Incubating, static-only, unapproved. Snapshot schema v1 and S2 delta, lineage, and bounded query contracts at `0.1` are implemented. Runtime correlation, scheduler overlays, private editor transports, and Vityo UI remain out of scope.

The long-term vocabulary in `docs/design/Styio-Observable-Language.md` does not override this wire schema.

## Files

Public contract library `styio_observable_core` (no compiler-internal or third-party JSON implementation libraries):

| File | Owns |
|------|------|
| `Snapshot.hpp/.cpp` | Schema-v1 snapshot value model, canonical compact serializer, `finalize_snapshot` / `parse_snapshot`, snapshot identity `s1_` plus 32 hex. |
| `JsonSupport.hpp` | Self-contained compact JSON writer, parser, and SHA-256 helper used only by the public serializer. |
| `Delta.hpp/.cpp` | Incubating delta envelope, linear-merge generation, transactional apply, lineage construction. |
| `Query.hpp/.cpp` | Bounded query request/response, negotiation, reference evaluator, immutable index shards. |
| `Service.hpp/.cpp` | Per-qualified-scope `ObservableTopologyService` façade: publish, retain, invalidate, degrade. |

Compiler-side producer adapters that translate a validated topology artifact into the public snapshot live in the sibling `StyioObservableProducer/` directory and are linked only into the full compiler. They are not part of `styio_observable_core`.

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
      ]
    }
  }
}
```

Rules:

1. The object is optional. If absent, the compiler allocates no snapshot model, JSON buffer, or profiler phase.
2. Only `schema_version` and `required_capabilities` are allowed. Any other field is `observable_static_snapshot_malformed`.
3. `schema_version` must be the integer `1`.
4. `required_capabilities` is an array of unique non-empty strings drawn from the closed set below. An empty array is admitted and still publishes the full closed set.
5. Admission requires `generated_by.tool = "pafio"`, exactly one package record matching `entry.package_id`, a namespaced package `name`, absolute package `root` and `manifest`, and canonical package-relative slash paths for the manifest and entry file.
6. Direct-file `generated_by.tool = "styio"` is `observable_static_snapshot_direct_file`. Any other producer is `observable_static_snapshot_styio_produced`.

## Snapshot output

Successful publication writes compact JSON with one trailing newline to:

`<artifact-dir>/<output-stem>.observable-static-snapshot.json`

The path is appended to `receipt.json` `artifacts`. A publication or write failure emits no partial snapshot file and uses diagnostic subcode `observable_static_snapshot` or a more specific `observable_static_snapshot_*` admission subcode.

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

Full `--machine-info=json` reports `observable_static_snapshot.schema_versions = [1]` and the snapshot capability list. Nano reports empty arrays.

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

Snapshot schema, delta schema, lineage capability, and query protocol are independently versioned. Negotiation requires equal major, selects the highest common minor, intersects optional capabilities, and rejects unknown required capabilities.

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

Checked-in corpus: `tests/fixtures/observable-topology/` (`manifest.json` plus parent/child/delta/lineage/query families covering unchanged, add, remove, field-change, rename, move, split, merge, partial, malformed, and unsupported). Schema-v1 snapshot goldens remain at `tests/fixtures/observable_static_snapshot/v1/`.

`tests/observable_topology_consumer_test.cpp` links only `styio_observable_core`. It negotiates, deserializes, applies deltas, and queries without compiler, editor, runtime, or LLVM implementation libraries.

An independent decoder can be written from this README and those fixtures alone.

## Privacy

Snapshots, deltas, lineage, queries, fixtures, errors, and counters must not contain workspace or package roots, absolute paths, raw source or literal values, content hashes, graph labels, pointer/address text, compiler object IDs, dense graph IDs, `source_text`, `raw_value`, credentials, or backend runtime records.

See the service inventory in [../MANIFEST.md](../MANIFEST.md).
