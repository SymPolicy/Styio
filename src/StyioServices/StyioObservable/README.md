# StyioObservable

**Purpose:** Own the incubating, compiler-produced schema-v1 static snapshot contract that adapts one Sema `ValidatedArtifact` into privacy-preserving JSON for independent consumers.

**Last updated:** 2026-09-05

**Status:** Incubating, static-only, unapproved. This stage is not a Pafio feature, not a stable schema, and not an external product capability. Delta, query, lineage, runtime correlation, and Vityo UI remain out of scope.

This README is the implementation contract a decoder can implement against, together with `tests/fixtures/observable_static_snapshot/v1/`. The long-term vocabulary in `docs/design/Styio-Observable-Language.md` does not override this schema.

## Files

| File | Owns |
|------|------|
| `StaticSnapshotContract.hpp/.cpp` | Schema-v1 constants, capability set, snapshot model, identity domains, evidence validation, and canonical serializer (`publish_validated_topology`, `publish_proven_scalar_noop`). |
| `StaticSnapshotPublication.hpp/.cpp` | CLI stage wiring called by `src/main.cpp`: `static_snapshot_machine_info_json()` for the `--machine-info=json` advertisement, `static_snapshot_identity_scope(...)` for the qualified Sema identity scope, and `publish_compile_plan_static_snapshot(...)` which runs the admitted stage after Sema and before lowering, writes the artifact, appends it to the receipt artifact list, and records the profiler phase and counters. Failures return one message attributed to subcode `observable_static_snapshot`. Nano advertises empty arrays and links no publication code. |

## Request

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

## Output

Successful publication writes compact JSON with one trailing newline to:

`<artifact-dir>/<output-stem>.observable-static-snapshot.json`

The path is appended to `receipt.json` `artifacts`. A publication or write failure emits no partial snapshot file and uses diagnostic subcode `observable_static_snapshot` or a more specific `observable_static_snapshot_*` admission subcode.

## Top-level key order

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

Collections are sorted by their identity field (`id` or `ref`) and contain unique identities. Compatible additive fields may appear after these keys; schema-v1 consumers ignore unknown fields. Unknown critical behavior named by completeness, schema version, or a missing required capability must fail closed.

## Capabilities

The closed, sorted advertised set is:

- `file-source-anchors`
- `producer-evidence`
- `static-topology-edges`
- `static-topology-facts`
- `static-topology-nodes`

Full `--machine-info=json` reports `observable_static_snapshot.schema_versions = [1]` and that capability list. Nano reports empty arrays. Existing machine-info fields keep their previous meaning.

## Completeness

| Value | When | Shape |
|-------|------|-------|
| `complete/validated-topology` | Sema published a `ValidatedArtifact` | Program-rooted nodes, edges, facts, file anchors, and evidence |
| `complete/proven-scalar-noop` | Sema recorded the scalar no-op lifecycle | `root` is JSON `null`; `nodes`, `edges`, `facts`, `anchors`, and `evidence` are empty arrays; no graph is constructed |

Any other completeness string is unsupported.

## Identifiers

Public IDs are versioned prefixes plus 32 lowercase hex characters (first 128 bits of SHA-256 over a length-prefixed preimage):

| Kind | Prefix | Domain |
|------|--------|--------|
| node | `n1_` | PLAN-003 qualified semantic identity (`styio.semantic-resource-node.v2`) |
| edge | `e1_` | compilation unit + edge kind + endpoint public IDs + relation key |
| fact | `f1_` | compilation unit + subject node ID + predicate (not value) |
| anchor | `a1_` | compilation unit + entry-relative path |
| evidence | `v1_` | compilation unit + producer rule + version + subjects + prerequisites + anchors |

Dense graph indexes, AST pointers, labels, absolute paths, raw source, content hashes, and compiler object addresses are not public IDs.

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

## Anchors

Schema v1 uses file precision only. Source-owned nodes share one anchor whose `path` is the compilation-unit entry-relative path and whose `precision` is `file`. Synthetic nodes omit anchors. Line and column are not published.

## Evidence

Each record has `ref`, `producer_rule`, `rule_version` (`"1"`), sorted unique `subjects`, `prerequisites`, and `anchors`.

- Node evidence: `styio.sema.topology.node.<Role>`
- Edge evidence: `styio.sema.topology.relation.<Kind>.<relation-key-or-default>` with both endpoint node-evidence records as prerequisites
- Fact evidence: `styio.observable.static.fact.normalize.<predicate>` with the subject node evidence as prerequisite

Relation keys are a closed compiler-owned vocabulary (for example `write-data`, `close-owner`, `resource-method`, `scope-exit-drop`); source identifiers such as user-defined resource method names never appear in `producer_rule` or anywhere else in the snapshot.

The graph is a DAG. Unresolved references, duplicate identities, incomplete descriptors, and cycles fail closed and emit no JSON.

## Privacy

Snapshots must not contain workspace or package roots, absolute paths, raw source or literal values, content hashes, graph labels, pointer/address text, compiler object IDs, or dense graph IDs. Documented package names and relative file anchors remain.

## Independent consumer

`tests/observable_static_snapshot_consumer_test.cpp` links GTest and LLVM JSON only. It accepts `canonical.json` and `additive-field.json`, and fails closed on `unsupported-schema.json`, `unsupported-completeness.json`, `missing-capability.json`, `dangling-reference.json`, and `cyclic-evidence.json`.

See the full service inventory in [../MANIFEST.md](../MANIFEST.md).
