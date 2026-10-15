# Current-source task/resource repair evidence

**Purpose:** Record the initial repair evidence and point to the reviewed production implementation.

**Last updated:** 2026-10-11

**Observed:** 2026-10-11, amended 2026-10-11 for the U2 canonical-name continuation
**Owner:** NODE-REPAIR / TASK-001
**Registered check:** `styio-task-resource-focused`

This is engineering evidence for the existing programme, not candidate acceptance
or Task/Tree delivery. The worker repaired the initial B1–B4 reproductions. Independent review found
additional ownership, scheduling and observation defects in that candidate;
[HANDOFF.md](HANDOFF.md) records their repairs and final candidate evidence. No commit, push, pull
request, installation, publication, or full regression was performed; the final
regression remains reserved for the later integration phase outside this graph.

## Candidate inputs and build

- Baseline revision `09f8fbdf6af9e27e590def6731eaa660d595a36c`, same reused
  branch `agent/downstream-styio-docs-20261005` and `build/default` directory.
- Pre-existing tracked modifications (`docs/INDEX.md`, `docs/plan/INDEX.md`,
  `docs/plan/README.md`) and untracked `docs/plans/` are preserved untouched.
- Incremental builds of `styio`, `styio_test`, `styio_resource_topology_test`,
  `styio_observable_runtime_test`, `styio_main_contract_test`,
  `styio_codegen_internal_test` completed with exit 0 on the same macOS arm64
  Debug configuration. No other platform, release, sanitizer, ICU, or
  Tree-sitter-OFF variant was verified.

## Executed checks

| Command or selector | Actual outcome | Evidence under `build/task-resource-repair-20261011/` |
| --- | --- | --- |
| `bash docs/plans/task-resource-delivery/checks.sh focused` | Exit 0: 6 run_styio_case fixtures, 110 passed and 2 platform-skipped `styio_test` cases, 57 topology cases, 1 contract case, 6 runtime cases | `focused-checks.log` |
| `ctest -R 'state_resources\|task_resources'` | 39 passed, exit 0: 23 state + 16 task cases including all new ones | logged in `focused-checks.log` session |
| `styio_main_contract_test` (all 45 tests) | 45 passed, exit 0 | process output; suite registered in `tests/CMakeLists.txt` |
| `styio_test` filter `StyioDiagnostics.MachineInfo*:StyioDiagnostics.CompilePlan*:StyioResourceEffects.*` | 126 passed, 2 skipped (pre-existing platform-conditional skips), exit 0 | direct run |
| Probe programs (`history-wrap`, `nested-empty`, `nested-write-separated`, `order-reverse-edge`, `order-reverse-await`, `f64-precision`) | All exit 0 with expected outputs below | `probe-exits.txt`, `<name>.stdout/.stderr` |
| B5 continuation: `styio --compile-plan package-identity/repro/plan.json` with canonical Pafio package fields | Before repair: exit 6, `STYIO_SERVICE_COMPILE_PLAN_INVALID` / `observable_static_snapshot_anonymous` / `packages[].root`. After repair: exit 0, snapshot artifact, receipt and runtime-events written; `compilation_unit` = `example.app` / `Styio.toml` / `src/main.styio` | `package-identity/repro-notes.txt`, `repro-diagnostic.jsonl`, `repro.exit`, `fixed.exit`, `canonical-snapshot.json`, `canonical-receipt.json` |
| B5 continuation: incremental target build (`styio`, `styio_lspd`, `styio_test`, `styio_resource_topology_test`, `styio_main_contract_test`, `styio_observable_runtime_test`, `styio_codegen_internal_test`, `styio_observable_static_snapshot_test`, `styio_observable_static_snapshot_consumer_test`) | Exit 0 on the same macOS arm64 Debug configuration | `package-identity/build.log` |
| B5 continuation: `styio_observable_static_snapshot_test` (affected suite outside the registered focused entrypoint) | 28 passed, exit 0, including `CanonicalPackageFieldsAreRequired` and the corrected `compile_plan_json` producer cases | `package-identity/snapshot-test.log`, `snapshot-test.xml` |

### Probe outcomes

| Probe | Repaired output | Baseline output | Reading |
| --- | --- | --- | --- |
| `task-history` fixture | `9 / 7` | `9 / 0` | B1: task observes creation-time committed history. |
| `history-wrap.styio` | `13`, `[9,10]` | captured sum `0` | B1 wraparound preserved: task captured `[6,7]`, parent history `[9,10]`. |
| `nested-empty.styio` | `7,7,7,9` | `7,7,9,9` | B2: empty child no longer commits the parent's pending write. |
| `nested-write-separated.styio` | `7`, `[0,0,7,11]`, `[0,7,11,9]` | parent `9` committed at child boundary | B2: child's `11` commits at child exit; parent's `9` commits at parent exit. |
| `order-reverse-edge.styio` | `2,1,3` | `1,2,3` | B3: `second => first` now enforces happens-before instead of being dropped. |
| `order-reverse-await.styio` | `1,2,3` | `1,2,3` | B3: edge direction honoured; output unchanged because it already agreed. |
| `f64-precision.styio` | `1.250000`, `1.110223` | same | C1 f64 precision preserved; unchanged. |

### Compile-plan per-invocation freshness (B4)

`CompilePlanDiagnosticsAndReceiptStayFreshPerInvocation` in
`tests/main_contract_test.cpp` covers success → parse failure → repeated parse
failure → repaired success → invalid intent → malformed array-root plan on a
single reused output root. Receipts appear only for successful invocations;
`diagnostics.jsonl` contains only the current invocation's records. The manual
probe sequence is preserved under `c1-sequence/`.

## Current repair implementation

The following describes the final working source. Earlier count-marker and
per-task-lock implementations were replaced during review; they are not
supported alternative paths.

- **B1 creation-time history:** `CodeGenIO.cpp` captures referenced committed
  rings and their heads. Numeric arrays use bulk copies into the task context
  and task-local storage. Strings transfer explicit allocation ownership;
  list/dict/matrix graphs are deep-copied at creation and moved into the worker
  handle table. Ordinary captured owned strings also outlive the parent scope.
  Task-local RAII and runtime returned-string cleanup settle success and failure.
- **B2 nested pending ownership:** `CodeGenG.cpp` uses a lazy pending array per
  scope, restoring the parent's array at scope exit. Child wraparound cannot
  overwrite the parent's pending value. Only the current scope's writes commit;
  cleanup releases each scope's pending owned values. Reads within a pulse keep
  the existing committed-value semantics.
- **B3 task order:** semantic analysis requires task handles and retains the
  existing topology cycle diagnostic. Lowering holds same-block after-targets;
  `SIOTaskOrder` is wired through the IR walker, verifier, printer, type visitor,
  codegen and runtime. Creation-time capture still precedes deferred execution.
  Runtime graph mutation and launch use one graph lock, direct ABI cycles are
  rejected, and completion retains an executor reference while releasing
  successors. Demand launch includes held ancestors. Worker dispatch uses
  nonblocking bounded-queue insertion with work-first execution when full.
- **B4 current invocation outputs:** `main.cpp` invalidates an old receipt before
  diagnostic setup and truncates the current diagnostics stream. Recoverable
  compile-plan admission failures perform the same invalidation. Successful
  invocations alone write receipts. A plan with no recoverable output identity
  cannot identify an old result; caller-side invocation ownership is required.
- **Observation:** queue waits carry the observation session epoch; a wait end
  consumes its ID and is emitted only in the matching session. The test clock is
  atomic, and the permanent wait-pairing test includes a prior session.
- **B5 canonical package identity (continuation):** the compile-plan consumer
  derived observable compilation-unit identity from `packages[].root` and
  `packages[].manifest`, but Pafio owns the compile-plan v1 producer contract
  and emits `packages[].root_dir` and `packages[].manifest_path`
  (`contracts/compile-plan/v1/compile-plan.schema.json`; `BuildPackageJson`).
  A real `pafio check --emit-observable-static-snapshot` invocation was
  rejected with `STYIO_SERVICE_COMPILE_PLAN_INVALID` /
  `observable_static_snapshot_anonymous` / `packages[].root`. The consumer now
  requires the canonical field names in
  `admit_qualified_compilation_unit`; the same admission guards runtime
  observation. No alias, fallback reader, or schema version was added, and the
  unchanged `observable_static_snapshot_anonymous` subcode still classifies as
  `STYIO_SERVICE_COMPILE_PLAN_INVALID`. Unrelated `root`/`manifest` names in the
  snapshot output document (`root`, `compilation_unit.manifest_path`) and the
  nano package-manifest domain are different contracts and were not changed.

Incidental defects surfaced by re-registering the previously unbuilt
`tests/main_contract_test.cpp` target: `json_require_string` dropped `field_path`
on the missing-field branch (fixed — `packages[0].id` is now reported correctly);
three stale assertions were updated (BSD `script` syntax, `blob listing` error
text, runtime-events `build_root` creation order).

## U2 continuation: canonical `namespace/name` package identity

Pafio runtime `798ff72acf150dcfa0891999c7a18e53` found U2 after B5: real
observation requests still failed. Pafio `src/PafioManifest/Manifest.cpp`
`NameRegex` owns the package-name grammar
`^[a-z0-9][a-z0-9_-]*/[a-z0-9][a-z0-9_-]*$` — each segment starts with a
lowercase ASCII letter or digit, continues with lowercase letters, digits,
`_` or `-`, joined by exactly one `/`. Styio rejected every slash in two
places: `package_name_is_valid` in `CompilePlanContract.cpp` (admission) and
`package_name_is_path_shaped` in `SemanticIdentity.cpp` (`Scope::qualified`,
reached at `main.cpp` before semantic analysis). Any canonical
`acme/task-resource` name was therefore refused at admission and could never
reach publication.

The repair introduces one shared linear validator,
`styio::semantic_identity::canonical_package_name_is_valid`, implementing the
owner grammar byte-for-byte (locate the single `/`, validate both segments in
one pass). Admission and `Scope::qualified` both call it; names are consumed
unchanged afterwards — no transform, alias, fallback reader, or schema version.
The admission diagnostic is now `compile-plan package name must match
namespace/name` under the unchanged `observable_static_snapshot_anonymous`
subcode. `compilation_unit.package_name` remains an opaque logical identity in
snapshot bytes, length-prefixed identity preimages, delta comparison and
service scope keys; the whole request path was traced and no consumer rejects
`/`. Unrelated domains — snapshot `root`, `compilation_unit.manifest_path`,
nano manifests, native `manifest.txt` — are unchanged.

Positive test producers and fixtures that carried unqualified names
(`example.app`, `compiler-tests`, `project`, `other-project`) were corrected to
canonical forms (`example/app`, `compiler/tests`, `example/project`,
`other/project`). Delta fixture `parent_snapshot_id`/`target_snapshot_id`
envelopes were recomputed from the corrected snapshot bytes, and the
producer-owned `canonical.json`/`additive-field.json` goldens were regenerated
through the suite's designed regen flow. The Worker initially added invalid-name coverage at both gates: eight rejected
producer-side names in
`RequestMatrixFailsBeforeSema`, and a twelve-case invalid plus three-case valid
(including `example-/app-name_2`) grammar table in
`CanonicalModuleRulesAndQualificationAreExplicit`. The B5
`root_dir`/`manifest_path` correction and its earlier evidence are retained
unchanged; `example.app` in the B5 repro records is historical output, not the
current grammar.

### U2 executed checks

All paths are relative to `build/task-resource-repair-20261011/`; the Pafio
gate scratch project and its isolated `PAFIO_HOME` (`pafio-gate/project`,
`pafio-gate/home`) live under the same ignored directory.

| Command or selector | Actual outcome | Evidence |
| --- | --- | --- |
| Incremental build: `styio`, `styio_lspd`, `styio_observable_static_snapshot_test`, `styio_observable_static_snapshot_consumer_test`, `styio_observable_consumer_test`, `styio_observable_delta_test`, `styio_observable_query_test`, `styio_observable_service_test`, `styio_observable_runtime_test`, `styio_resource_topology_test`, `styio_lowering_internal_test`, `styio_typeinfer_internal_test`, `styio_test`, `styio_main_contract_test` | Exit 0, macOS arm64 Debug, existing `build/default` | build output; `logs/` per-binary logs |
| `styio_observable_static_snapshot_test` | 28 passed (includes regenerated golden lock and the new admission invalid-name cases) | `logs/styio_observable_static_snapshot_test.log` |
| `styio_observable_static_snapshot_consumer_test`, `styio_observable_consumer_test`, `styio_observable_delta_test`, `styio_observable_query_test`, `styio_observable_service_test` | 8 + 3 + 11 + 4 + 5 passed; delta fixtures re-locked to recomputed identities | `logs/*.log` |
| `styio_resource_topology_test` | 57 passed; one worker fixture error (`example-/app` wrongly listed as invalid — it is valid per the owner grammar) corrected before rerun | `logs/styio_resource_topology_test.log` |
| `styio_lowering_internal_test`, `styio_typeinfer_internal_test` | 26 + 45 passed | `logs/*.log` |
| `styio_observable_runtime_test` under registered env `STYIO_TASK_QUEUE_CAPACITY=2 STYIO_TASK_THREADS=1` | 25 passed; the same binary run without the constrained env leaves queue-capacity-sensitive cases red, which is the registered contract, not a regression | `logs/styio_observable_runtime_test.log` |
| `styio_test` filter `StyioResourceTopology.*:StyioResourceEffects.*:StyioStreamZip.*:StyioDiagnostics.MachineInfo*:StyioDiagnostics.CompilePlan*` | 140 passed, 2 platform skips | `logs/styio_test_focused.log` |
| `styio_main_contract_test` | 45 passed | `logs/styio_main_contract_test.log` |
| Actual Pafio producer: `styio-interface-gate.py --task-resource --json` selecting this rebuilt `build/default/bin/styio` and the existing `pafio-nightly build-codex/bin/pafio` (producer revision `a20dce6e9d889e892457b45b14d8d745e686ed34`), scratch project + isolated `PAFIO_HOME` under ignored build | Worker-reported gate exit 0; retained JSON records 30 steps with expected exits: real `acme/task-resource` admission, `compilation_unit` = `acme/task-resource`/`pafio.toml`/`src/main.styio`, initial snapshot, parent-snapshot check with receipt-recorded empty delta, missing-parent `full_snapshot_required`/`parent_unreadable` with identical snapshot bytes, observed `run` with session.capability/summary framing and `task.created`/`task.completed`/`task.result_consumed` and no `task.failed`, compile-error exit-23 envelopes, dry-run, two concurrent builds with distinct output ownership, per-step old-artifact preservation | `logs/pafio-gate.json`, `logs/pafio-gate.stderr`; artifacts under `pafio-gate/` |
| Observation-off receipt inspection (corrected by reviewer) | 14 successful unobserved receipts in the retained Worker run, no snapshot/delta artifacts; disabled controller JSONL remains. Six successful observed receipts are separate. The earlier 15 count was incorrect. | Retained gate artifacts; reviewer `producer-review.json` |

The host subsequently reported `styio-task-resource-focused` passed in check
run `8e93839c08907eed0f6ab829a127be27`; full console and per-command exits are
not exposed by the assigned plan API. Independent review completed the scoped
source/evidence inspection described in HANDOFF.md. It added a dedicated
23-invalid/five-valid name table for each admission mode, expanded independent
semantic-scope coverage, and removed the smaller overlapping request-matrix
loop. The reviewer recorded an incremental CLI/LSP/affected-test build (exit 0),
two affected admission tests (exit 0), and one semantic identity test (exit 0).
Existing containment, unmatched/duplicate identity, relocation, snapshot,
delta and runtime results remain reusable while their inputs are unchanged.

The original gate orders its parent check before observed execution. Review
added only the uncovered post-runtime check against the first retained snapshot
and then an observation-off transition, both exit 0 on the same scratch and
existing Pafio executable. The empty delta, receipt-listed files, relative
identities, runtime snapshot/site IDs and task settlement were independently
verified; earlier artifacts stayed byte-identical. No valid producer sequence
or whole focused suite was replayed. Eleven authoritative fixture/expectation
files are retained in the same scratch. Full reviewer commands, outputs and
true exits are under `build/task-resource-handoff-20261011/canonical-name/`.
Worker logs retain their original GoogleTest results but no separate exit files;
review does not infer process exits from that text. The reported 31 gate steps
and 15 unobserved receipts are corrected above to 30 and 14.

Only the coordinator resumes Pafio admission and admits the Vityo join. No full
regression, commit, push, PR, install or sibling rebuild was performed.

## Permanent tests added

| Test | Defect covered |
| --- | --- |
| `state_resources/t13_task_history_capture` + `t17_task_capture_repeated_reads` | B1 history capture, repeated snapshot reads |
| `state_resources/t14_task_history_wraparound` | B1 wraparound |
| `state_resources/t15_nested_pending_scope` + `t16_nested_commit_order` | B2 nested commit ownership |
| `task_resources/t07_task_order_edge`, `t08_task_order_chain`, `t09_task_order_failed_predecessor` | B3 edge, chain, failure settlement |
| `task_resources/e06_task_order_non_task`, `e07_task_order_cycle` | B3 non-task handle rejection, self-edge cycle rejection |
| `StyioObservableRuntime.TaskOrder*` (6 cases in `tests/observable_runtime_test.cpp`) | B3 held launch, multi-predecessor gating, pull backstop, post-completion edge, failed predecessor release, invalid/self/launched-edge rejection |
| `StyioMainContract.CompilePlanDiagnosticsAndReceiptStayFreshPerInvocation` | B4 per-invocation diagnostics/receipt freshness |
| `StyioObservableStaticSnapshotAdmission.CanonicalPackageFieldsAreRequired` (`tests/observable_static_snapshot_test.cpp`) | B5: full canonical Pafio package record admitted for snapshot and runtime-observation requests; legacy `root`/`manifest` names rejected with `observable_static_snapshot_anonymous`; unobserved plan unaffected |
| `StyioObservableStaticSnapshotAdmission.PackageNameGrammarForBothObservationModes` (`tests/observable_static_snapshot_test.cpp`) | U2 reviewer completion: five valid and 23 invalid names independently specified for static and runtime-only admission; canonical names and opaque package IDs preserved, stable rejection diagnostics asserted |
| `StyioSemanticIdentity.CanonicalModuleRulesAndQualificationAreExplicit` grammar table (`tests/resource_topology_test.cpp`) | U2: independent semantic-scope table expanded during review for both starts, `@`, control whitespace and non-ASCII, with digit-start positive coverage |

## Reviewer follow-up and limits

The initial worker classified `WaitEpisodesPairReasonsSubjectsAndResolutions`
as a pre-existing non-blocking flake. That disposition was incorrect for this
scope. Review reproduced the orphan WaitEnd, repaired session ownership and
passed the observable runtime module, including the original strict assertions.
The initial task snapshot also lacked owned-resource lifetime coverage; the
additional cases and their real CLI/native runs are recorded in
[HANDOFF.md](HANDOFF.md).

The initial six-fixture focused entrypoint has been replaced with permanent
state/task CTests, LSP framing, relevant compiler/contract/codegen groups and
the observable runtime module. Its old successful log applies to the command
as executed then, not to the expanded entrypoint. Historical overlapping runs
are not summed into a task-wide total. Platform skips remain skips.

No multiwriter merge, shared resource alias syntax, block-local resource
declarations, external-I/O rollback or cancellation semantics were introduced.
No full regression, common Pafio/Vityo E2E, other platform/build variant or
Task/Tree delivery is established by this evidence. Final candidate provenance,
source review, additional permanent cases and downstream conditions are in
[HANDOFF.md](HANDOFF.md).
