# PLAN-004 Publish immutable observable topology snapshots

Phase: ready · Revision: unsealed

This document is a render-only projection of `Plan.json`. Edit `Plan.json`; never edit this file.

## Intent

**Goal**: Publish compiler-owned, versioned, immutable observable topology snapshots from the existing Sema-owned Resource Topology artifact without changing ordinary compiler behavior.

**In scope**
- Qualified-scope admission; snapshot value model; canonical ordering; node, edge, fact, evidence, source-anchor, and completeness schema; opaque identity encoding; privacy redaction; deterministic serialization; capability advertisement; producer fixtures; focused compatibility evidence; benchmark handoff requirements.

**Out of scope**
- Delta, query, lineage, incremental caches, runtime or scheduler hooks, Vityo UI, telemetry backend, policy engine, deterministic replay, package-manager behavior, cloud services, and unrelated language work.

**Success**
- An explicitly requested snapshot serializes deterministically and replays every published fact, carries version, capability, completeness, and evidence, contains no process pointer, machine path, raw source, or raw value, rejects anonymous global publication, and leaves ordinary compiler behavior unchanged.

**Risk boundary**
- Do not expose AST, IR, or graph layout as ABI; bind wire layout to mutable compiler internals; derive identity from host paths or source locations; weaken validation; add exporter dependencies to normal compilation; or claim stable 1.0 compatibility before producer and consumer fixtures pass.

## Decisions

Dossier status: not_required

No non-discoverable user decision was required.

### Observed repository facts

- none

## Requirements

| Code | Statement | Sources |
| --- | --- | --- |
| REQ-001 | Publish only from the immutable `ValidatedArtifact` produced once by Sema and already reused by lowering; do not rebuild, scrape, or reinterpret topology in the CLI or serializer. | Design.md |
| REQ-002 | Admit public snapshot publication only for a compile-plan request whose producer and package metadata establish one unambiguous qualified package/entry compilation unit. Direct-file compilation, anonymous scopes, malformed metadata, and unmatched package entries must fail the requested artifact explicitly. | Design.md |
| REQ-003 | Define compilation-unit scope from the existing namespaced package name plus normalized package-relative manifest and matched entry-relative paths. Absolute filesystem paths may be used only to validate containment and compute those relative paths; they are never persisted. | Design.md |
| REQ-004 | Give every published node, edge, fact, anchor, and evidence record a deterministic versioned opaque identifier. IDs remain stable across machines and repeated equivalent builds, while dense graph IDs remain process-local implementation details. | Design.md |
| REQ-005 | Exclude absolute paths, content hashes, raw source text or values, memory addresses, compiler object IDs, display labels, source locations, and incidental traversal order from public identity derivation. | Design.md |
| REQ-006 | Define one immutable, move-only, schema-versioned static snapshot contract with an explicit incubating stability marker, producer metadata, compilation-unit metadata, declared capabilities, completeness, root, nodes, edges, facts, anchors, and evidence. | Design.md |
| REQ-007 | Publish the validated resource nodes and edges, normalized semantic facts for node capabilities and type state, and enough stable kind/role vocabulary for an independent consumer without exposing the private graph representation. | Design.md |
| REQ-008 | Emit normalized package-relative file anchors where source ownership is known and a producer-evidence DAG that identifies the compiler rule and rule version responsible for every node, edge, and fact. Synthetic items may omit anchors but not evidence. | Design.md |
| REQ-009 | Distinguish a complete validated topology from a complete proven scalar-noop result. Never silently publish a partial graph; inability to prove requested completeness is an artifact error. | Design.md |
| REQ-010 | Serialize compact UTF-8 JSON with fixed object-key order, canonically sorted collections, one trailing newline, no unordered-container dependence, and bounded auxiliary memory suitable for large graphs. | Design.md |
| REQ-011 | Add snapshot publication as an absent-by-default compile-plan emission request. Ordinary compilation and compile plans that do not request it must preserve current lifecycle, diagnostics, lowering, receipt, output, and performance behavior. | Design.md |
| REQ-012 | Advertise supported schema versions and capabilities in full-compiler machine information, require callers to select a supported schema version, reject unknown required capabilities before compilation, and keep nano capability reporting explicit and empty. | Design.md |
| REQ-013 | Provide canonical golden fixtures plus an independent JSON consumer that validates schema/version/capability/completeness rules without linking compiler internals. Prove additive-field tolerance and fail-closed handling of unsupported critical capability requests. | Design.md |
| REQ-014 | Preserve PLAN-002's single Sema artifact, scalar-noop fast path, and lowering reuse, and preserve PLAN-003's internal dense-ID behavior while completing one deliberate migration of qualified semantic scope with no legacy compatibility path. | Design.md |
| REQ-015 | Expose one opt-in profiler phase with item counts and serialized bytes, document reproducible workloads and measurements for the external benchmark owner, and do not add thresholds or edit the external benchmark repository in this stage. | Design.md |
| REQ-016 | Keep the plan draft and unapproved. Exclude delta snapshots, query/index APIs, lineage/history, runtime or scheduler events, Vityo, backend/cloud transport, policy/replay, package-manager changes, and unrelated compiler work. | Design.md |

## Architecture

Add a full-compiler service adapter that converts Sema's private immutable `ValidatedArtifact` into a versioned immutable static snapshot only when a qualified compile-plan emission request is present. Migrate the qualified semantic scope once to package-and-entry logical identity, record stable public relation seeds while Sema builds the graph, derive opaque public IDs and evidence without exposing graph internals, and canonically serialize the resulting value. Keep all causally dependent implementation, fixture, contract, and documentation work in one Task so the plan has one parallel-safe Task frontier; use independent Nodes inside that Task for baselines, admission, identity, producer/consumer proof, compatibility proof, and documentation before a final focused join.

- Authority remains with the current compiler contracts and completed PLAN-002/PLAN-003. `docs/design/Styio-Observable-Language.md` is consulted only as a long-term vocabulary and boundary reference; it does not override current code or this plan.
- The adopted structural pattern is an Adapter: `StyioServices/StyioObservable` receives a narrow read-only publication view from `ValidatedArtifact` and produces a public `StaticSnapshot`. Direct `Graph` serialization is rejected because it would leak dense IDs, labels, AST pointers, and storage order. Memento is rejected because no mutable state is captured; Observer is rejected because publication is synchronous and opt-in; Strategy and Factory add no justified variation for one schema and serializer.
- Snapshot schema version 1 has a fixed top-level key order: `contract`, `schema_version`, `stability`, `producer`, `capabilities`, `compilation_unit`, `completeness`, `root`, `nodes`, `edges`, `facts`, `anchors`, `evidence`. Its stability is `incubating`, not stable.
- A node record contains `id`, `kind`, `role`, `anchors`, and `evidence`. An edge contains `id`, `kind`, `from`, `to`, and `evidence`. A fact contains `id`, `subject`, `predicate`, canonical JSON `value`, and `evidence`. An anchor contains `ref`, normalized `path`, and `precision`; version 1 precision is `file`. Evidence contains `ref`, `producer_rule`, `rule_version`, `subjects`, `prerequisites`, and `anchors`.
- Version 1 publishes one `capabilities` fact and one `type-state` fact per resource node, including an empty sorted capabilities array when appropriate. It does not publish raw graph labels, AST text, literal values, or internal addresses. The root is the persistent Program node ID for validated topology; a proven scalar-noop snapshot is complete with a null root and empty collections and does not construct a graph.
- The qualified compilation-unit key is `(namespaced package name, normalized manifest-relative path, normalized matched entry-relative path)`. Admission resolves `entry.package_id` against exactly one compile-plan package record, validates that manifest and entry are contained by that package root, and normalizes separators and dot segments before constructing the key. Version, target display name, absolute workspace/package roots, and host filesystem spelling are not identity inputs.
- Qualified `SemanticIdentity::Scope` is migrated in place from the old project/package plus logical-module pair to the compilation-unit key. Its derivation domain is versioned anew and the old qualified overload, preimage, and documentation are removed in the same change. Anonymous scope remains available internally for direct-file compilation but is never publicly comparable or publishable.
- Public IDs are lowercase fixed-width 128-bit values with a short versioned class prefix. Node IDs are the migrated PLAN-003 semantic IDs. Edge IDs derive from scope, edge kind, endpoint persistent IDs, and a closed semantic relation key captured at the Sema edge-creation site. Fact IDs derive from subject ID and predicate, not fact value. Anchor IDs derive from scope and relative path. Evidence IDs derive from producer rule/version, subjects, prerequisites, and anchors. All encodings are length-prefixed and collision-guarded; consumers must treat every ID as opaque.
- Sema centralizes public edge seed recording beside its existing node identity helper. Publication material is private to `ValidatedArtifact`; raw `Graph` construction in low-level tests remains valid but cannot be published. Node evidence is keyed by the closed semantic-role producer rule, edge evidence depends on endpoint node evidence, and fact evidence depends on the subject node evidence. The adapter validates that all references resolve and the evidence graph is acyclic before serialization.
- Completeness is closed to `complete/validated-topology` and `complete/proven-scalar-noop`. A requested snapshot with anonymous or ambiguous metadata, unresolved publication seed, duplicate/colliding public identity, dangling reference, evidence cycle, or serialization/write failure is a named artifact failure; it is never downgraded to partial output.
- Canonicalization builds dense-to-public lookup vectors for O(1) endpoint translation, reserves all output vectors from graph counts, and sorts each public collection once by opaque ID. Fact values and string sets have explicit canonical ordering. Expected time is O(N log N) for emitted records and expected auxiliary memory is O(N); no repeated whole-graph scans or per-item map lookups are added.
- Compile-plan emission is an optional `emit.observable_static_snapshot` object with `schema_version` and `required_capabilities`. The object is strictly parsed. Only Pafio-produced plans with a matched qualified package entry are eligible in version 1; direct-file CLI use and Styio-generated compile plans remain anonymous and reject this requested artifact. Plans without the field follow the existing path byte-for-byte apart from inactive branch checks.
- Full-compiler machine information advertises schema version 1 and its closed capability set; nano reports no static-snapshot schema or capability. Unknown schema versions and unknown required capabilities fail before parsing or Sema work. Publication occurs after successful Sema and before lowering, writes `<output-stem>.observable-static-snapshot.json` through the existing artifact-output/receipt path, and does not allow the request to bypass existing requested stages.
- Fixture tests use a representative qualified package graph covering source, handle, stream operation, state, sink, task/failure semantics where currently produced, edge varieties currently emitted, canonical facts, file anchors, and evidence joins. A standalone JSON-only consumer validates references, version/capabilities, completeness, evidence acyclicity, and additive-field tolerance without importing compiler headers.
- The profiler adds one opt-in snapshot-publication phase with node/edge/fact/anchor/evidence counts and serialized bytes. Repository benchmark documentation defines invocation, fixture scale, warmup, repetitions, and fields for the external `styio-benchmark` owner; acceptance records data but introduces no stage threshold.

## Tasks

Every Task belongs to the same mutually independent parallel frontier.

### TASK-001 publish-qualified-static-topology-snapshot

Worker: general · Tier: complex · Workload: heavy · Verification: code · Frontier: parallel

Outcome: The full compiler can optionally publish one canonical, privacy-preserving, versioned, complete static snapshot derived from its existing Sema artifact for a qualified package entry, with independent consumer fixtures, compatibility proof, and benchmark handoff; the default compiler path is unchanged.

Risks: quality, privacy, performance, shared_resource
Requirements: REQ-001, REQ-002, REQ-003, REQ-004, REQ-005, REQ-006, REQ-007, REQ-008, REQ-009, REQ-010, REQ-011, REQ-012, REQ-013, REQ-014, REQ-015, REQ-016
Writes: src/StyioUtil/SemanticIdentity.hpp, src/StyioUtil/SemanticIdentity.cpp, src/StyioResourceTopology/ResourceTopology.hpp, src/StyioResourceTopology/ResourceTopology.cpp, src/StyioSema/SemaContext.hpp, src/StyioSema/TypeInfer.cpp, src/StyioLowering/AstToStyioIRLowerer.hpp, src/StyioServices/StyioConfig/CompilePlanContract.hpp, src/StyioServices/StyioConfig/CompilePlanContract.cpp, src/StyioServices/StyioObservable, src/StyioServices/StyioProfiler, src/cmake/StyioServicesSources.cmake, src/main.cpp, tests/resource_topology_test.cpp, tests/main_contract_test.cpp, tests/observable_static_snapshot_test.cpp, tests/observable_static_snapshot_consumer_test.cpp, tests/fixtures/observable_static_snapshot/v1, tests/CMakeLists.txt, src/StyioServices/MANIFEST.md, src/StyioServices/README.md, src/StyioServices/StyioConfig/README.md, docs/EXTERNAL-SERVICES.md, docs/design/Styio-Observable-Language.md, benchmark/README.md, docs/teams/PERF-STABILITY-RUNBOOK.md, docs/TEST-CATALOG.md, docs/plan/INDEX.md
Exclusive resources: semantic identity domain/version and qualified-scope API, ValidatedArtifact publication view, compile-plan snapshot emission schema, observable snapshot schema/version/capability names, public identifier domains, snapshot output filename and receipt artifact name, snapshot fixture directory, snapshot profiler phase

In scope
- Qualified compile-plan admission; one-time semantic-scope migration; stable node/edge/fact/anchor/evidence identity; private publication metadata in `ValidatedArtifact`; immutable snapshot model; canonical JSON serializer; producer evidence and completeness validation; full/nano capability reporting; opt-in CLI and receipt integration; focused unit/contract/golden/consumer tests; profiler seam; owner documentation and benchmark handoff.

Out of scope
- Delta/query/index/lineage/history APIs; runtime/scheduler events; Vityo; backend or cloud transport; policy and replay; package-manager or Pafio implementation changes; stable-schema declaration; content-addressed snapshots; AST-wide source-span retrofit; thresholds; unrelated compiler refactors.

Outputs
- OUT-001 Strictly validated package-and-entry logical compilation-unit value (`src/StyioServices/StyioConfig/CompilePlanContract.hpp`): One matched Pafio package record supplies namespaced package name plus normalized manifest-relative and entry-relative paths without exposing absolute paths.
- OUT-002 Migrated qualified semantic identity scope and versioned node derivation (`src/StyioUtil/SemanticIdentity.hpp`): Equivalent qualified package entries rebuild to the same opaque node identities and the superseded qualified derivation is removed.
- OUT-003 Immutable node and relation publication descriptors owned by the Sema artifact (`src/StyioResourceTopology/ResourceTopology.hpp`): The public adapter can derive complete stable relations without seeing Graph storage, AST pointers, labels, or dense IDs.
- OUT-004 Move-only schema-v1 snapshot model, identity domains, evidence validation, and canonical serializer (`src/StyioServices/StyioObservable/StaticSnapshotContract.hpp`): One compiler-owned adapter emits complete deterministic privacy-preserving bytes from the validated artifact or fails closed.
- OUT-005 Compile-plan request, machine capability negotiation, artifact output, receipt, and profiler wiring (`src/main.cpp`): Qualified requests publish after Sema and before lowering while an absent request allocates no snapshot state and preserves ordinary compilation.
- OUT-006 Canonical, relocated-root, additive-field, and invalid public JSON fixtures (`tests/fixtures/observable_static_snapshot/v1/canonical.json`): Producer and independent consumer share reviewable schema-v1 contract evidence without compiler-private dependencies.
- OUT-007 Producer, admission, privacy, lifecycle, compatibility, and independent-consumer tests (`tests/observable_static_snapshot_test.cpp`): Focused tests prove every publication invariant and the disabled compiler path before repository-wide regression.
- OUT-008 Service ownership, layering, version, capability, and failure contract (`src/StyioServices/README.md`): Maintainers have one current compiler-service boundary for static snapshot publication.
- OUT-009 Current implemented and deferred observable-language boundary (`docs/design/Styio-Observable-Language.md`): The long-term archive is reconciled with the implemented incubating static-only stage without becoming implementation authority.
- OUT-010 Reproducible opt-in workload and profiler-field handoff (`benchmark/README.md`): The external benchmark owner can measure item counts, bytes, time, and memory without a local threshold or external-repository edit.
- OUT-011 Focused verification and performance-stability maintenance rules (`docs/teams/PERF-STABILITY-RUNBOOK.md`): Future changes can rerun exact producer, consumer, privacy, disabled-path, and profiler evidence.
- OUT-012 Regenerated tracked Better Plan index (`docs/plan/INDEX.md`): Repository plan documentation lists PLAN-004 without hand-maintained generated state.

Internal Node graph
- NODE-001 freeze-snapshot-contract-and-fixtures · after: none · Freeze the incubating schema-v1 field order, kind/role/predicate spellings, capability set, completeness values, ID grammar, representative source fixture, canonical golden JSON, and independent consumer expectations before implementation branches consume them.
- NODE-002 capture-disabled-and-compiler-baselines · after: none · Capture default compile, qualified compile-plan, scalar-noop, machine-info, topology/lowering reuse, profiler, and representative output baselines so compatibility assertions compare observable behavior and not assumptions.
- NODE-003 migrate-qualified-package-entry-identity · after: NODE-001 · Replace qualified semantic scope and derivation with the package-name plus manifest-relative plus matched-entry-relative compilation-unit key; remove the old qualified derivation and extend deterministic/collision/privacy tests.
- NODE-004 implement-qualified-publication-admission · after: NODE-001 · Strictly parse the optional emission request, retain the matched package record fields needed for qualification, validate producer, uniqueness, containment, normalization, requested schema, and capabilities, and expose an explicit admitted-or-error request to the compiler driver.
- NODE-005 derive-immutable-snapshot-from-sema · after: NODE-003 · Record closed relation seeds at Sema edge creation, expose a narrow private publication view from the immutable artifact, derive and validate all public records/evidence/completeness, and canonically serialize without exposing the Graph.
- NODE-006 wire-opt-in-publication-and-negotiation · after: NODE-004, NODE-005 · Advertise full/nano capability differences, construct the qualified scope before Sema, publish after successful Sema and before lowering, integrate output/receipt/error handling, and add the opt-in profiler phase while leaving the absent-request branch inactive.
- NODE-007 prove-canonical-producer-and-independent-consumer · after: NODE-001, NODE-005 · Prove stable bytes and IDs across repeated builds and relocated absolute roots, validate sorted records/references/evidence/completeness, and run the standalone JSON consumer against golden and additive-field fixtures.
- NODE-008 prove-admission-privacy-and-disabled-compatibility · after: NODE-002, NODE-006 · Prove malformed/anonymous/ambiguous/unmatched requests and unsupported versions/capabilities fail before Sema, scan artifacts for forbidden data, and compare no-request behavior and scalar-noop behavior with captured baselines.
- NODE-009 prove-topology-lowering-and-profiler-compatibility · after: NODE-002, NODE-006 · Prove one Sema artifact remains the source for both snapshot and lowering, dense IDs remain internal, no second graph is built, existing topology/lowering tests remain valid, and opt-in profiler counters and serialized-byte accounting are coherent.
- NODE-010 update-owning-contracts-and-benchmark-handoff · after: NODE-004, NODE-005, NODE-006 · Update service manifests, compile-plan and observable-language contracts, external-service boundaries, test catalog, performance runbook, benchmark handoff, and current plan index while keeping the stage incubating, unapproved, and externally unimplemented.
- NODE-011 verify-focused-static-snapshot-closure · after: NODE-007, NODE-008, NODE-009, NODE-010 · Run the focused build/tests/gates once all branches join, inspect the canonical artifact and receipt, and record evidence for every requirement without authorizing the plan or running the full repository regression.

Design
- approach
  - Begin with contract fixtures and measured baselines because schema spelling, public ID domains, and disabled-path observations are inputs to every implementation and proof branch. Keep fixtures hand-reviewable and large-scale performance generation separate from contract goldens.
  - Resolve compile-plan package metadata at the existing contract boundary. Require exactly one package record matching `entry.package_id`, a valid namespaced package name, package root containment for manifest and entry files, and canonical relative paths. Pass only the resulting logical compilation unit into Sema and the snapshot adapter; discard absolute path material from the public model.
  - Migrate qualified semantic identity atomically. Use a new domain string and length-prefixed compilation-unit components, preserve anonymous internal use, remove the old qualified constructor and tests, and update all qualified callers in the same Node. This intentionally changes qualified PLAN-003 node IDs once; version 1 snapshot fixtures become the new public baseline.
  - Extend the Sema-owned artifact with immutable publication descriptors rather than exposing `Graph`. Centralize edge creation through a Builder helper that records a stable semantic relation key adjacent to the edge. Reject publication from manually assembled raw graphs or any artifact lacking complete publication descriptors.
  - Implement `StaticSnapshotContract` under `StyioServices/StyioObservable` as the sole Adapter and serializer owner. Build records in reserved vectors, translate dense endpoints through one vector, collision-check all identity classes, validate reference closure and evidence acyclicity, sort once, and emit keys explicitly in schema order through LLVM JSON-compatible primitives without relying on map iteration order.
  - Derive file-level anchors only. The compiler currently lacks an authoritative AST-wide public span contract, so schema version 1 uses the qualified entry-relative path with `precision: file` for source-owned items and no anchor for synthetic items. Do not synthesize line/column precision or add a parser-wide source-location refactor to this stage.
  - Keep evidence deterministic and bounded: node evidence names the closed Sema producer rule for its semantic role; edge evidence names the relation rule and lists both endpoint evidence records as prerequisites; fact evidence names the normalization rule and lists the node evidence prerequisite. Validate a DAG and sorted unique references before serialization.
  - Parse `emit.observable_static_snapshot` only in the full compile-plan contract. Reject unsupported schema or required capability immediately, before source parse/Sema. If absent, do not construct scope publication descriptors, snapshot records, or JSON buffers. If present and admitted, a failure is attributed to the named requested artifact and participates in existing receipt failure semantics.
  - Add machine-info capability data without changing existing capability meaning: full reports schema version 1 and the closed snapshot capability set; nano reports empty support. Treat additive JSON fields as ignorable for schema-v1 consumers unless a new behavior is named in `required_capabilities`; never reinterpret unknown critical behavior silently.
  - Test producer and consumer separately. Producer tests may inspect compiler APIs and golden bytes; the standalone consumer reads fixture JSON only and validates public contract invariants. Relocation tests compile equivalent package trees under distinct absolute roots and require identical snapshot bytes. Privacy tests scan decoded key/value strings as well as raw bytes.
  - Reuse the existing optional profiler seam for one snapshot phase and aggregate counts/bytes only when requested. Document benchmark commands and handoff fields locally; the external benchmark owner decides workloads, collection, and thresholds in a later authorized change.
- patterns
  - pattern_catalog: refactoring-guru-catalog-22-v1
  - candidate: Adapter
  - decision: adopt
  - pressure: The public schema must consume compiler-owned semantic truth without exposing mutable Graph storage, dense IDs, labels, or AST pointers.
  - expected_benefit: One adapter owns schema versioning, public identity derivation, completeness/evidence validation, canonical ordering, and serialization while Sema and lowering retain their current ownership.
  - simpler_alternative: A direct `ValidatedArtifact::to_json` method is smaller in file count but couples the private graph layer to a public wire contract and makes privacy/version boundaries difficult to enforce.
  - application: `StyioServices/StyioObservable::StaticSnapshotContract` adapts a narrow immutable publication view from `ValidatedArtifact` into a move-only public snapshot value and canonical bytes.
  - costs_and_rejections: The adapter adds one immutable intermediate O(N) model; Memento, Observer, Strategy, and Factory are rejected because this stage has no mutable capture, asynchronous fan-out, runtime variant selection, or construction family.

Acceptance
- AC-001 covers REQ-003, REQ-004, OUT-001, OUT-002
  - Given Equivalent qualified Pafio package entries built repeatedly under different absolute workspace roots, plus variants changing one logical identity component
  - When compilation-unit and public IDs are compared
  - Then equivalent logical builds have identical compilation units and node, edge, fact, anchor, and evidence IDs, while package name, manifest-relative path, entry-relative path, semantic role, relation, or subject changes the relevant versioned identity
  - Oracle: qualified relocation and identity-domain tests pass with exact ID-set comparisons
  - Evidence: command from qualified compilation-unit and relocated-root focused tests
- AC-002 covers REQ-002, REQ-012, OUT-005
  - Given Direct-file, Styio-produced, anonymous, malformed, duplicate, ambiguous, unmatched, escaping-path, unsupported-version, and unsupported-capability requests
  - When compile-plan admission runs
  - Then each requested artifact fails before source parse or Sema with a specific diagnostic, while one valid Pafio package entry is admitted
  - Oracle: compile-plan admission matrix and pre-Sema instrumentation pass
  - Evidence: command from snapshot request contract tests
- AC-003 covers REQ-001, REQ-006, REQ-007, REQ-008, REQ-010, OUT-003, OUT-004, OUT-006, OUT-008
  - Given A representative validated resource topology containing all currently produced node and edge varieties
  - When the Sema artifact is adapted and serialized twice
  - Then the same artifact supplies a Program-rooted immutable schema-v1 snapshot with complete nodes, edges, capability/type-state facts, file anchors, producer evidence, fixed key order, sorted unique collections, resolved references, an acyclic evidence DAG, and byte-identical compact JSON
  - Oracle: canonical producer golden and artifact-source tests pass
  - Evidence: command from static snapshot producer and golden tests
- AC-004 covers REQ-009, REQ-014, OUT-004, OUT-007
  - Given An admitted scalar-only compile and injected missing descriptor, dangling endpoint, duplicate/collision, evidence-cycle, and write-failure cases
  - When publication is requested
  - Then scalar no-op emits complete/proven-scalar-noop with null root and empty collections without constructing a graph, and every invalid case emits no partial artifact
  - Oracle: scalar completeness and fail-closed publication tests pass
  - Evidence: command from completeness and artifact-failure focused tests
- AC-005 covers REQ-005, REQ-008, OUT-003, OUT-006
  - Given Qualified snapshots and their receipt entries produced from fixtures containing path-shaped labels and literal values
  - When raw bytes and decoded keys and values are scanned
  - Then no workspace/package root, absolute path, raw source or literal value, content hash, graph label, pointer/address, compiler object ID, or dense graph ID is present, while documented package names and relative file anchors remain
  - Oracle: privacy scan and decoded-contract assertions pass
  - Evidence: command from snapshot privacy tests and forbidden-token scan
- AC-006 covers REQ-011, REQ-014, REQ-001, OUT-005, OUT-007
  - Given Captured default, no-request compile-plan, scalar-noop, topology/lowering, output, receipt, machine-info legacy-field, and profiler baselines
  - When the implementation runs with snapshot emission absent
  - Then lifecycle, diagnostics, outputs, lowering reuse, and profiler observations remain unchanged, no snapshot model or buffer is allocated, and no second topology is built
  - Oracle: baseline comparisons and one-build structural instrumentation pass
  - Evidence: command from disabled-path and topology/lowering compatibility matrix
- AC-007 covers REQ-012, REQ-013, OUT-006, OUT-007
  - Given Full and nano machine information plus canonical, additive-field, unsupported-schema, unsupported-completeness, missing-capability, dangling-reference, and cyclic-evidence JSON fixtures
  - When the standalone JSON-only consumer runs
  - Then full advertises schema v1 and its capabilities, nano advertises none, compatible additive fields are accepted, unsupported critical behavior fails closed, and the consumer validates public references without compiler headers
  - Oracle: machine-info contract and independent-consumer suites pass
  - Evidence: command from main contract and observable snapshot consumer tests
- AC-008 covers REQ-015, REQ-016, REQ-014, OUT-009, OUT-010, OUT-011, OUT-012
  - Given The one-time qualified-scope migration, profiler seam, and owning documents are complete
  - When topology/semantic-identity compatibility tests, focused repository gates, and documented profiler workload run
  - Then dense IDs and lowering remain internal and unchanged, counts and serialized bytes are coherent, external benchmark handoff is reproducible without thresholds or external edits, all owner documents mark schema v1 incubating/static-only/unapproved, and generated plan indexing is current
  - Oracle: focused compiler compatibility, profiler, architecture, docs, local-info, hygiene, and diff gates all exit zero
  - Evidence: command from focused closure commands and benchmark handoff dry run

Focused regression
- ``cmake --preset dev``
- ``cmake --build --preset dev -j2 --target styio resource_topology_test main_contract_test observable_static_snapshot_test observable_static_snapshot_consumer_test``
- ``ctest --preset dev --output-on-failure -R 'resource_topology_test|main_contract_test|observable_static_snapshot_test|observable_static_snapshot_consumer_test'``
- ``python3 scripts/architecture-layer-gate.py``
- ``python3 scripts/docs-local-info-scan.py``
- ``python3 scripts/hygiene-scan.py``
- ``python3 scripts/diff-quality-gate.py``
- ``test -z "$(rg -n '/(Users|home|private|tmp)/|0x[0-9A-Fa-f]{6,}|sha(1|256)|content[_-]?hash|raw[_-]?source' build tests/fixtures/observable_static_snapshot/v1 || true)"``
- paths: `src/StyioUtil`, `src/StyioResourceTopology`, `src/StyioSema`, `src/StyioLowering`, `src/StyioServices/StyioConfig`, `src/StyioServices/StyioObservable`, `src/StyioServices/StyioProfiler`, `src/main.cpp`, `src/cmake/StyioServicesSources.cmake`, `tests`, `tests/fixtures/observable_static_snapshot/v1`, `src/StyioServices`, `docs`, `benchmark`

## Full regression

Run inside the sole Reviewer session after every repair is integrated.

- ``cmake --preset dev``
- ``cmake --build --preset dev -j2``
- ``ctest --preset dev --output-on-failure``
- ``python3 scripts/architecture-layer-gate.py``
- ``python3 scripts/docs-local-info-scan.py``
- ``python3 scripts/hygiene-scan.py``
- ``python3 scripts/diff-quality-gate.py``
- ``git diff --check``
- paths: `src`, `tests`, `scripts`, `docs`, `benchmark`, `CMakeLists.txt`
