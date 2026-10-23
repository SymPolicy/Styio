# Reviewed CLI/LSP candidate

**Purpose:** Maintain the reviewed CLI/LSP evidence and the finite FINAL/CLOSE execution handoff for the existing joint milestone.

**Last updated:** 2026-10-16

**Evidence observed:** 2026-10-11–12. **Review scope:** NODE-FINAL → NODE-CLOSE / TASK-001.
**Registered checks:** FINAL selects only `styio-task-resource-final`; CLOSE selects only `styio-task-resource-review`.

The current execution contract is below. Earlier B1–B5/U1/U2/U3 compiler,
editor and real Pafio producer records retain their historical scope; their
then-pending external prerequisites do not supersede the coordinator's later
acceptance. The independent reconciliation below supplies the actual final regression and
scoped review evidence; it does not claim joint delivery. The current delivery status is recorded in the [coordinator join and delivery closure](#coordinator-join-and-delivery-closure--2026-10-15); statements describing source Task/Tree delivery as unfilled are historical and superseded by that closure.

## FINAL/CLOSE execution plan

Use the same hosted source `TASK-001` in this Tree. Current authority is
the assigned objective, the coordinator's blackboard decision, `Tree.json`,
`tasks/TASK-001.json`, `../README.md` and the existing owner contracts. Read this HANDOFF and
`../pafio-nightly/docs/plans/task-resource-delivery/HANDOFF.md` and
`../vityo-nightly/docs/plans/task-resource-delivery/HANDOFF.md` from the repository
root. Those sibling candidates are read-only, consumed in place; no rebuilds.

The coordinator accepted all three source reviews and the same-candidate
production-service/vityod → Pafio → Styio join. Vityo retains the complete
evidence under `build/task-resource-integration/`. Planning inspected its
`gate-report.json` (`ok=true`, `skipped=false`) and `process.json` (exit 0).
The coordinator accepted the 35 required real cases and four boundaries;
unknown child skips remain unknown. Do not rerun this E2E unless a specific
changed input invalidates its evidence, and report that impact to coordinator.

| Execution lane | Dependencies | Finite outcome and check |
| --- | --- | --- |
| FINAL Worker, source `NODE-FINAL` | Planning bootstrap; accepted common join above | Prepare current-source inputs and complete ignored logging; hand control to HOST for only `styio-task-resource-final`; address actual scoped failures with affected/remaining recovery. |
| Independent CLOSE Reviewer, source `NODE-CLOSE` | Same planning bootstrap **and** FINAL, including actual host check evidence | Independently inspect final code, migration, findings and complete evidence; reconcile local requirements and handoff; select only `styio-task-resource-review`. |

The hosted execution runs under the assigned plan; it maps to existing source
TASK-001, not a new source Task/Tree.
Use current non-fast roles. FINAL exclusively owns this repository's scoped
writes, then yields ownership to CLOSE. No extra execution lanes, subordinate
agents, model changes or arbitrary time/silence/iteration/attempt cutoffs.
Preserve the dirty checkout, `build/default`, accepted baseline/focused/E2E
evidence, independent permanent source assets and user data.

FINAL preparation inspected the initial `checks.sh final`, which ran
`cmake --build build/default` followed by
`ctest --test-dir build/default --output-on-failure --no-tests=error` under
`set -euo pipefail`. Retain these build/test and failure semantics. Arrange
complete stdout/stderr logs, CTest's detailed test records and exact process exits
under ignored `build/task-resource-final/` before returning control to HOST. Keep
the first run's CTest records before affected reruns can overwrite them. Preserve exits
through any tee/pipeline; the bounded host console tail is not the complete
record. Record ordinary Git revision/status/diff and untracked inputs, selected
executables, build configuration, actual commands and relevant environment once;
reuse existing evidence where inputs are unchanged. No whole-tree hashing,
cache framework, speculative checks or sibling source snapshots. Do not manually
run the full suite first or mark the check passed before actual host evidence.

If that run fails, keep its original logs/results. Diagnose actual failures,
repair ordinary scoped defects directly and rebuild affected targets. Before
returning control for the host's automatic same-ID follow-up, execute that
follow-up once against only the required affected/remaining commands; the
registered `checks.sh final` entrypoint keeps its complete-suite form and is
never rewritten to the recovery selection. Document the failed commands,
invalidated inputs, reused valid passes, repaired-case coverage and any remaining
checks/skips. Keep every required check accounted for and permanent product gates
effective; do not remove failures, silently skip work, retry without diagnosis or
invent generic recovery machinery. The current task explicitly authorizes these
ordinary repairs; the older generic final-regression approval paragraph in
`docs/specs/POST-COMMIT-CI-CHECKS.md` does not require another approval here.
Its input-validity and evidence-reuse rules still apply. For an outside-scope
contract/risk change, investigate facts and alternatives, recommend a concrete
option with impact and validation, and hold only dependent work.

CLOSE independently reads the actual code and complete logs, reconciles the
host result plus any affected follow-up, and closes scoped findings with focused
validation only. The registered review command is patch hygiene, not semantic
review. Check required producer/consumer/document migration in one-off inspection;
retain permanent independent assets and remove superseded authorized paths.
Map BASELINE's supported-capability inventory to actual results without treating
its declared feature labels or configured test count as executed coverage. Report
explicit platform/feature skips, unsupported boundaries and unverified release,
installed-UI and real-Agent acceptance. In particular, `didSave` is not implemented
LSP save semantics. Do not sum overlapping runs or infer totals from truncated logs.
Each privacy finding needs a redacted file/line/rule/category/basis/impact/disposition
row; raw paths, payloads and logs remain ignored.

Amend existing Node/check records and this HANDOFF using actual evidence, retaining
the first failure and later disposition. The stopping artifact is this repository's
independently reviewed candidate and reconciled evidence, including any cross-repo
impact for coordinator. Do not wait within an Agent turn for sibling final nodes:
they share the Agent lease. Leave source TASK-001 and Tree delivery unset;
coordinator alone joins all three results and closes source/hosted Tasks and Trees.
Do not close the organization programme or activate a later milestone. No commits,
push, PR, installation, publication, real Agent conversations or installed-UI
acceptance. This is bounded local engineering evidence, not an all-platform or
zero-defect assertion.

## Independent CLOSE reconciliation — 2026-10-12

The Reviewer read the current authority, blackboard, planning handoff and all
three HANDOFF records, then independently inspected the final scheduler,
capture/pending-scope paths, task-order producer/IR/consumer wiring, canonical
compile-plan identity and receipt setup, grammar changes and permanent tests.
The only CLOSE code edit is the existing internal scheduler test's ownership
cleanup. Production sources and the selected CLI/LSP remain unchanged from the
successful host recovery. Scoped findings and their dispositions follow.

| Finding | Independent disposition and evidence |
| --- | --- |
| C1: `tests/externlib_internal_test.cpp:702–773` retained raw deletion after ready publication and a synthetic ready/notify thread | Migrated this existing test to `close_task` owner release and real pending-task launch. A blocked callback now deterministically asserts two live references, protecting the original missing executor-reference defect. Raw deletion could race `run()`'s terminal access; synthetic publication bypassed real settlement. Only this test target was rebuilt, exit 0; the corrected named case passed. |
| C2: recovery omitted two CLI task failure paths | `StyioResourceEffects.TaskAwaitFallbackRunsAfterFailedTaskPull` and `FailedTaskAwaitWithoutFallbackStopsAtSettlementSite` both execute the relinked CLI. Ran these two previously uncovered affected checks with the corrected C1 case: three passed, no failures/skips, exit 0. The host's 135-case pass remains its actual selection, not an invented broader run. |
| C3: stale final records and refreshed input provenance | Reconciled actual host checks and complete XML/logs in the existing Node and handoff. `prepared-inputs/provenance.json` is captured at 2026-10-11 20:22:41 UTC during recovery; it is not the original 16:33 full-run source patch. No missing first-run bytes are reconstructed. Original commands, script, configuration, executable stats, full results and failure records remain intact. Current production patch matches the recovery record; CLOSE's test delta is retained separately. |
| C4: direct-enqueue boundary was overstated as an ABI | Source inspection finds the scheduler class in an unnamed namespace. Exported spawn calls go through `stash_spawned_task` / `styio_task_request_launch`; direct enqueue calls outside graph launch are in the internal test. Documentation now reflects that distinction. |

### Actual final evidence

The assigned host plan reports FINAL validation passed after an initial failed
stage. Full evidence is under ignored
`build/task-resource-final/`. These are separate overlapping selections, never
an added total. CTest's success percentage includes skips; the counts below
come from every named record in complete XML, checked against complete verbose
and copied `Testing/Temporary/LastTest.log` records.

| Execution | Actual result | Retained evidence |
| --- | --- | --- |
| HOST first unfiltered incremental build and full CTest | Build 0; CTest/check 8; retention 0. 1756 distinct cases: 1747 passed, four failed, five skipped. | `host-eQXYCDzm/`: complete build/CTest logs, command and exit files, XML, script, cache and copied `Testing/`. The failure is preserved as failed. |
| HOST affected/remaining recovery through the same registered check | Build/CTest/retention/check all 0. 135 distinct cases: 134 passed, one skipped. All four original failures passed. | `host-sjCaPEkJ/`: same complete record set. `latest.txt` selects this invocation. |
| CLOSE affected build and three-case selection | `styio_externlib_internal_test` build exit 0; three named tests passed, zero failures/skips, CTest exit 0. | `close/build-process.json`, `build.log`, `affected-process.json`, `affected.log`, `affected.xml` and copied `Testing/`. |
| CLOSE evidence and migration inspection | Exit 0; parsed complete records, checked unique names/exits/log membership, active cutover wiring, actual LSP link dependencies and current input equality. No product suite executed by this inspection. | `close/inspect_evidence.py`, `process.json`, `evidence-review.json`, `migration-review.json`, `input-review.json`, `current.patch`. This one-off script is ignored, not a permanent gate. |

Recovery changed the executor-retain location in `ExternLib.cpp`, two stale
security assertions and the generated task-order feature evidence. CLOSE then
changed only the internal scheduler test. The 135 host cases revalidate the
selected extern/runtime/security/task/state paths; C1 supersedes just its altered
test and C2 adds the missing two CLI paths. First-run successes on unchanged
parser, sema, lowering, codegen, observation schema/consumer, CLI-contract, IDE,
package, native/FFI and non-task feature paths are reused. No failing check was
removed and no product gate was weakened. The registered final entrypoint runs
the complete build and unfiltered suite; the recorded recovery was a one-shot
affected/remaining execution under the same check ID.

### Supported-capability reconciliation

BASELINE's 46 feature declarations (44 labelled converged, two not started)
and its old configured count are an inventory, not passes. Actual final evidence
maps the declared surfaces as follows; named cases and all per-case outcomes
remain in `close/evidence-review.json`.

| BASELINE surface | Actual coverage and boundary |
| --- | --- |
| Core syntax, scalars/containers, binding/control/imports | First-run feature fixtures, `StyioFiveLayerPipeline`, parser/internal, type inference, IR and security cases passed, with the two corrected task-order contract cases supplied by host recovery. No universal grammar-completeness claim. |
| Callable inference/constraints/defaults/captures/effects/interfaces/cache | First-run `callable_*`, affine/literal fixtures, callable graph/effect/usage and specialization/native-cache cases passed. Three callable task-rejection fixtures also passed in recovery. Rank-2 callbacks and user-extensible constraints remain not-started features. |
| Bounded history and nested pending-write ownership | State-resource fixtures passed in recovery, including creation-time history, wraparound, nested ownership, repeated reads, owned strings/matrix, parent-scope exit, failure cleanup and f64 precision. Native matrix history passed. Structural topology/copy/typestate cases from the full run remain valid. |
| Task creation/order/await/cleanup | Recovery task fixtures and runtime lifecycle/order/capture/security cases passed, including reverse native order, failed predecessor, general-cycle rejection and fan-out under bounded queue pressure. CLOSE supplies C1/C2. No cancellation, detached-task or new alias/multiwriter semantics. |
| File/stdin/stdout, cleanup/effects/fallback | First-run file/stdio and resource-effect cases passed except the explicit unavailable-device skips below. CLOSE revalidated both CLI task-failure settlement cases. No external-I/O rollback claim. |
| Streams/pulses/ranges/zip | First-run `stream_processing_*`, `wave_dispatch_*`, `StyioStreamZip`, topology and zip-barrier cases passed; state-resource recovery covers the changed pending-scope behavior. Future merge/pressure capabilities are not promoted by these tests. |
| CLI, machine-info, plans, diagnostics, receipts, observation | First-run `StyioDiagnostics`, 45 main-contract cases and static snapshot/admission/delta/query/service/consumer cases passed; runtime observation passed again in recovery. Canonical producer identity and invocation-owned outputs retain accepted integration evidence. |
| IDE and LSP | First-run syntax/semantic/service/server/runtime tests, five authoritative task/resource syntax cases, malformed/restored parity and both stdio cases passed. Tree-sitter ON only. `didSave` is unadvertised/unhandled; transport-state preservation is not LSP save semantics. The release latency-budget case was skipped. |
| Native/FFI, Nano, algorithms, security | First-run native interop/toolchain/internal/platform cases, `StyioNano`/`StyioNanoPackage` and 187 C++ reference-equivalence cases passed in this configuration. These are local tests, not real registry publication, a release package or remote CI. |
| Platform/configuration | macOS arm64 Debug, LLVM 18, Tree-sitter ON, ICU OFF and Nano ON. Linux, Windows, other architectures, ICU ON, Tree-sitter OFF, Release, sanitizers, installed UI and real-Agent acceptance remain unverified. |

The actual five distinct skipped cases are:

- `StyioResourceLifecycle.FlexFileRebindCleanupFailureStopsBeforeOverwrite`
- `StyioResourceEffects.NamedCleanupHandlerRunsForFileCloseFailure`
- `StyioResourceEffects.IoHandlerDoesNotCatchFileCloseCleanupFailure`
- `StyioSafetyRuntime.FileCloseFailureIsCleanupRuntimeEffect`
- `StyioIdePerf.EnforcesFrozenLatencyBudgets`

The first four require unavailable `/dev/full`; the fourth was encountered
again in recovery and remains the same skipped obligation. The last requires
the Release performance harness. None is counted as passed. Machine-info still
lists cancellation, cooperative suspend, cooperative/I/O/resource/timer waits as
unavailable. Compiler task/history tests do not establish those capabilities.

REQ-STYIO-BASELINE/REPAIR/SNAPSHOT/ORDER map to the reviewed source and named
actual results above; RESEARCH retains COST's bounded i64 comparison and numeric
bulk-copy result, without new measurements or a native speedup claim.
REGRESSION is the first full execution plus justified recovery and C1/C2
validation. E2E retains the coordinator's accepted 35-real/four-boundary evidence
with the input-impact explanation below. BOUNDARY/INTEGRITY preserve independent
assets, the single active implementation and explicit verification limits.

### Input validity and coordinator handoff

The CLI changed during FINAL recovery; the LSP did not. CLOSE verified their
current stats match `host-sjCaPEkJ/executables-after-build.json`; CMake cache is
unchanged. The actual LSP link command has no runtime-core dependency, so the
scheduler edit does not require a daemon rebuild. No sibling was changed or built.

Moving the executor retain from `claim_task_launch` to `enqueue` keeps one
executor reference on every production spawn/held/dependency launch before work
can execute. Owner release waits for ready and drops its own reference; the
executor survives through notify and successor dispatch. Direct internal test
calls now gain the missing reference. The shared history/f64/failure programs,
codegen, compile-plan protocol, Pafio consumers and Vityo document wiring are
unchanged. The accepted production-service/vityod E2E remains reusable for its
exercised behavior; this is a source-based impact conclusion supported by
current task/runtime/CLI results, not a new byte-identical-candidate E2E run.
Coordinator must reconcile the changed selected CLI identity in sibling input
admission records. No sibling final wait or complete E2E replay is performed here.

The scoped record inspection parsed plan JSON, found no trailing whitespace in
the four CLOSE-edited source records, confirmed ignored evidence and unchanged
Task/Tree delivery fields (`close/record-checks.json`). Repository local-info
rules scanned 158 changed tracked/unignored new files with no findings
(`close/privacy.json`); later result-only additions were manually reviewed.
The affected documentation audit exited 0 (`close/docs-process.json`, full
`close/docs.log`); no product tests were rerun to populate these records.
Raw paths, complete runtime output and current-source patch stay ignored.

The existing review check selection remains only `styio-task-resource-review`.
Its HOST execution follows this handoff; result and run ID stay unset until
observed. Source TASK-001 and Tree delivery remain unset for coordinator join.
The stopping artifact is this reviewed local candidate and evidence, not joint
programme delivery, publication, installation or all-platform/live acceptance.

## Historical FINAL host regression preparation — 2026-10-12

The selected input remains revision `09f8fbdf6af9e27e590def6731eaa660d595a36c`
plus the current dirty working tree, on the existing task branch and
`build/default`. FINAL changed only this handoff, its existing Node evidence
and the milestone's final-check logging. No compiler, test, fixture, generated
grammar or sibling source was changed. All three HANDOFF records, current
Tree/Task authority, BASELINE inventory and the planning handoff were read.
The accepted Vityo join's retained `gate-report.json` has `ok=true` and
`skipped=false`; `process.json` has exit 0. That evidence was read, not replayed.

Exact local inputs are retained under ignored
`build/task-resource-final/prepared-inputs/`: ordinary Git revision/branch/status,
tracked binary-capable diff, untracked input copies, the existing CMake cache and
compilation database when present, selected executable stats and tool versions.
This is the dirty candidate record, not a whole-tree archive or content-hash gate.
The selected configuration remains macOS arm64 Debug, Unix Makefiles,
Tree-sitter ON, ICU OFF and Nano ON. CLI/LSP inputs remain
`build/default/bin/styio` and `build/default/bin/styio_lspd`; siblings remain
selected in place without writes or builds.

HOST must execute only `styio-task-resource-final`, through
`bash docs/plans/task-resource-delivery/checks.sh final`. Its build remains
`cmake --build build/default`, followed on success by unfiltered
`ctest --test-dir build/default --output-on-failure --no-tests=error`.
The added `--verbose` and `--output-junit` arguments affect reporting only
([CTest documentation](https://cmake.org/cmake/help/latest/manual/ctest.1.html)).
No selection, timeout, retry, skip policy or permanent gate was changed.

Each host invocation allocates a distinct ignored `host-*` directory under
`build/task-resource-final/`, named by `latest.txt`. It retains the executing
script, exact commands, allowlisted relevant environment, tool selection,
CLI/LSP stats before and after the build, resulting CMake cache, complete
`build.log` and verbose `ctest.log`, individual command exits and `check.exit`.
`ctest.xml` supplies structured outcomes; the copied `Testing/` directory
preserves CTest's detailed records before recovery can replace them. Copy output
and its exit are retained separately. Direct redirection avoids pipeline exit
masking: a failed build prevents CTest, and a failed CTest keeps its own exit
even if evidence copying also fails. A retention failure cannot produce a pass.
Console tails are only navigation aids; reconcile from these complete files.

| Preparation evidence | Actual scope |
| --- | --- |
| `preparation/bash-syntax.log/.exit` | `bash -n` passed; shell syntax only. |
| `preparation/logging-probe-current.log/.exit` and `logging-probe.json` | Three isolated stub-command fixtures passed: success, build exit 23 with no test invocation, and test exit 8 with detailed logs retained. The final script was copied into ignored fixture roots; no real compiler build or product test was launched. |
| `preparation/privacy.json/.exit` | Repository local-info rules scanned the three FINAL source records with no findings; subsequent result-only additions were manually reviewed. Raw environment, paths and logs stay ignored. |
| `preparation/static-checks.json` and `whitespace-corrected.json` | Initial untracked-file whitespace inspection found an existing trailing-space line here; removed it and the affected check has no findings. No-index exit 1 denotes content difference; exit 3 and the original diagnostic are retained in the first record. |

Paths in that table are relative to `build/task-resource-final/`. These checks
establish logging/exit behavior only. The full registered regression has **not
run during FINAL preparation**; its check result and run ID remain pending/null.
No case total or platform skip outcome is inferred before host evidence.

After an actual host failure, preserve that directory, diagnose the named
failures and repair ordinary scoped defects. Run the same check ID once against
explicit affected targets and affected/remaining tests as the recovery
follow-up; the registered final entrypoint itself keeps its complete-suite
commands. Record exactly which first-run passes retain
valid inputs, which are invalidated, and which checks remain; do not replay the
whole suite or silently omit failed checks. No recovery selection is guessed now.

No current production-input invalidation was introduced by preparation. CLOSE
must inspect the actual host build for any changed selected executable and
reconcile its relevant inputs before reporting cross-repository impact. It owns
independent semantic/migration/capability review after host evidence; it must not
repeat valid E2E or wait for sibling final nodes. Existing unavailable runtime
capabilities, the unimplemented LSP `didSave` boundary, other platforms and
configurations, release and live/installed acceptance remain explicit limits.
Source TASK-001 and Tree delivery remain unset for the coordinator's final join.

## FINAL first-run failure diagnosis and scoped recovery — 2026-10-12

The host's first `styio-task-resource-final` run completed the full suite and
failed 4 of 1756 checks; complete evidence is retained under
`build/task-resource-final/host-eQXYCDzm/`. The current `latest.txt` selects the
subsequent host recovery, not this retained first run. Five platform/feature
skips were also recorded; see below. The failures, diagnoses and repairs:

| Failed check | Diagnosis | Repair |
| --- | --- | --- |
| `StyioExternLibInternal.TaskSchedulerAndPendingTaskReleaseEdgesStayExplicit` (subprocess killed after ~3 h) | `StyioTask::run()` decrements `references` and self-deletes at zero, but the executor reference was only granted by `claim_task_launch`. Tasks enqueued directly through the internal `StyioTaskScheduler::enqueue` (the direct calls in this test) ran with a single owner reference, were freed inside `run()`, and the caller's later `ready` wait double-referenced freed memory — a use-after-free that hung instead of crashing. | `src/StyioExtern/ExternLib.cpp`: the executor reference is now claimed in `enqueue` itself, the single funnel every executed task passes through, and removed from `claim_task_launch`. Claim-path tasks hold the same total count as before; direct `enqueue` callers now also hold an executor reference through `run()`'s final access. |
| `StyioIRContract.LegacyAndDeclarationAstLoweringEdgesStayExplicit` | The contract test still asserted the pre-B3 `=>` lowering (`SGNoOp`). The accepted task-order feature deliberately lowers `ResourceOrderAST` to `SIOTaskOrder`; semantic rejection of non-task operands is the new contract, covered end-to-end by `task_resources/e06`, `t07`–`t10`. | `tests/security/styio_security_test.cpp`: the `ResourceOrderAST` lowering edge now asserts `SIOTaskOrder`, matching the shipped contract. |
| `StyioTypeInferenceContract.LeafNoopAndFailClosedTypeInferNodesStayExplicit` | Same stale contract in type inference: `ResourceOrderAST` was listed among leaf no-throw nodes, but `=>` now requires task handles (`"task order `=>` requires task handles"`). | Same file: removed `ResourceOrderAST` from the leaf list; added explicit positive (`task[i64]`-bound operands pass) and negative (`i64` operands throw `StyioTypeError`) coverage of the `=>` contract. |
| `syntax_feature_state_gate` | The generated `docs/design/syntax/SYNTAX-FEATURE-GRAPH.json` was stale: the task-order feature doc now points its golden evidence at the new `t07_task_order_edge` fixture. | Regenerated with `python3 scripts/syntax-feature-state-gate.py --write` (3-line golden-evidence update); gate now reports ok. |

No permanent product gate was removed or weakened and no failing check was
silently dropped. `checks.sh final` keeps its full evidence infrastructure
(distinct `host-*` directory per invocation, invocation/environment record,
command files, complete `build.log`/`ctest.log`/`ctest.xml`, copied `Testing/`,
individual exit files, bounded console tails). The recorded recovery follow-up
under the same check ID executed only the affected build and the
affected/remaining test selection, not another full-suite pass; the registered
entrypoint retains its complete form for subsequent runs:

- Build: `cmake --build build/default --target styio styio_security_test styio_externlib_internal_test styio_observable_runtime_test`
- Tests: `ctest --test-dir build/default --output-on-failure --no-tests=error --verbose --output-junit <logdir>/ctest.xml -R 'StyioExternLibInternal\.|StyioObservableRuntime\.|StyioSafetyRuntime\.|StyioSecurityBoundedTaskScheduling\.|StyioSecurityNightlyRuntime\.|LegacyAndDeclarationAstLoweringEdgesStayExplicit|LeafNoopAndFailClosedTypeInferNodesStayExplicit|^(task_resources|state_resources)_|callable_capabilities_err_e02_task_instance|callable_capabilities_err_e07_copy_task_instance|callable_values_err_e04_captured_scheme|^syntax_feature_state_gate'` (135 entries)

### Coverage reconciliation

Changed inputs and the checks they invalidate:

- `src/StyioExtern/ExternLib.cpp` rebuilds `styio_runtime_core` (relinking
  `styio_core` consumers). The only behavioral change is the executor-reference
  claim point inside the task scheduler. First-run passes are invalidated only
  for checks that execute `enqueue`/`run()`/task-close paths: the externlib
  internal binary, `StyioObservableRuntime.*`, `StyioSafetyRuntime.*`,
  `StyioSecurityBoundedTaskScheduling.*`, `StyioSecurityNightlyRuntime.*`,
  feature fixtures that spawn tasks (`||>` literals, `=>` order edges, `?|`
  pulls — all under `task_resources_*` and the task-capture subset of
  `state_resources_*`; both labels rerun in full for a bounded, unambiguous
  selector), and the three `||>` fixtures under `callable_*` labels. On every
  production launch path (`styio_task_request_launch`/`styio_task_complete`)
  the reference count is net-identical to the first run; the changed reference ownership also covers direct internal `enqueue` callers.
  The scheduler class is in an unnamed namespace; it is not an exported ABI.
- `tests/security/styio_security_test.cpp` changes two assertions in the two
  failed contract tests; other cases in that binary keep unchanged expectations
  and unchanged exercised production paths.
- `docs/design/syntax/SYNTAX-FEATURE-GRAPH.json` invalidates only the
  `syntax_feature_state_gate` check; `syntax_feature_state_gate_unit` is
  included to keep the gate pair executed together.

Reused first-run passes: every check whose exercised inputs are unchanged —
lexer/parser/sema/lowering/codegen contract cases, observable static-snapshot/
delta/query/service/consumer suites, LSP/IDE entries, native-interop,
workflow, stream-processing, wave-dispatch, main-contract, and all feature
fixtures that do not spawn tasks. An incidental static-library relink is not by
itself an input change for a check that never executes the changed functions;
the reuse rule is applied per exercised path, not per archive member.

Remaining checks and skips: the four first-run failures and the invalidated
selection above are rerun by the same check ID. `StyioSafetyRuntime.
FileCloseFailureIsCleanupRuntimeEffect` (1152) reruns and remains a recorded
environment-conditional skip. The other four first-run skips (225, 289, 292,
1464) exercised unchanged paths and retain their recorded skip status; no skip
is counted as passed anywhere.

Worker-side verification (not the host check): the same affected/remaining
selection was run once locally after the repairs — 134 passed, 1 skipped
(1152, same conditional skip as the first run), 21.42 s. The previously
hanging scheduler test completed in 7 ms; both contract tests and both syntax
gate checks passed. That Worker execution was not the registered result. The actual subsequent
host recovery is reconciled above. First-run evidence in
`host-eQXYCDzm/` is preserved; the follow-up writes a fresh `host-*` directory.

Cross-repository impact: the `ExternLib.cpp` repair relinks
`build/default/bin/styio`, which the accepted Pafio/Vityo join consumed. On
the production launch path the repaired reference protocol is net-identical to
the tested build — produced tasks reach `enqueue` only through
`styio_task_request_launch`/`styio_task_complete`, and the executor reference
is now added inside `enqueue` with an identical net count — so the accepted
E2E evidence remains valid for the paths it exercised.
The binary identity changed; CLOSE independently reconciles whether any
consumer evidence needs revalidation and reports residual impact to the
coordinator. No sibling source or binary was written.

## Current U3 reviewed candidate

Vityo's real production document route found that the selected LSP reports
`STYIO_SERVICE_EDITOR_SYNTAX` on the authoritative task-history fixture that
the compiler executes successfully. The editor Tree-sitter grammar does not
cover the required existing syntax in the reported candidate. The U3 correction,
host focused result and independent review below establish scoped editor parity
for the current CLI/LSP candidate. Historical compiler, U2 and Pafio results
retain their stated scope. The coordinator owns resuming Vityo and its separate
production-service join; no Vityo rerun or final regression is claimed here.
No language feature expansion or Tree-sitter diagnostic suppression was used.

The host subsequently passed both U3 registered checks: the focused and the
review check. The coordinator inspected retained final
build/protocol exits and IDE XML, accepted this CLI/LSP candidate and completed
the current source REPAIR/HANDOFF check records. This permits resuming the
existing Vityo phase; it does not establish its production join or final regression.

### Historical U3 scope and execution context

This continuation uses the existing hosted source `TASK-001`, `NODE-REPAIR` and
`NODE-HANDOFF`. The scope has one Repair Worker and an independent Handoff
Reviewer; the host's registered focused check joins them. Earlier BASELINE/COST
records and B1–B5/U1/U2 evidence remain historical inputs, not new work.

Planning read the Vityo HANDOFF and source NODE-REPAIR account of that finding
and the evidence selected by
`../vityo-nightly/build/task-resource-repair/latest-documents.json`:
`document-diagnostics.json` contains 16 editor-syntax diagnostics. The original
Vityo blackboard entry is not exposed by this Styio-scoped interface; those
mirrored records and retained diagnostics are the inspected input. The
coordinator's Styio blackboard clarification also applies: immediate syntax
publication does not establish completion of request-driven semantic publication.

| Lane | Finite outcome and acceptance | Check ownership |
| --- | --- | --- |
| Repair Worker / NODE-REPAIR | Reproduce copied t13 over the real selected LSP protocol; trace compiler syntax; repair owner grammar/adapters; regenerate checked-in artifacts; add permanent fixture and malformed-edit coverage; build final CLI/LSP and affected tests; retain versioned protocol evidence | Worker runs uncovered IDE/grammar/protocol cases. Host alone runs `styio-task-resource-focused` after the Worker handoff. |
| Independent Handoff Reviewer / NODE-HANDOFF | Inspect final source/generated artifacts, fixture authority, actual evidence and host result; independently verify the shared fixture grammar; repair scoped findings, rebuild invalidated targets and rerun affected inputs; amend this handoff and both existing source Node records | Select only `styio-task-resource-review`. Its patch-hygiene result does not replace independent semantic review. Do not rerun the whole focused suite. |

The Worker has exclusive Styio write ownership until handoff; the Reviewer then
owns scoped corrections. Use the current non-fast Codex roles without subagents
or arbitrary task, time, silence or attempt limits. Reuse this dirty checkout and
`build/default`; preserve user changes and all earlier evidence. Sibling owner
contracts and evidence are read-only inputs. Do not rebuild sibling binaries.

Compiler acceptance remains owned by the nightly parser, as specified in
`docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md`. Before editing, follow the
relevant `src/StyioParser/Parser.cpp`, `NewParserExpr.cpp` and
`src/StyioToken/Token.hpp` rules and feature contracts. Scope the repair to these
authoritative shared sources and directly affected editor cases:

- `tests/features/state_resources/t13_task_history_capture.styio`
- `tests/features/state_resources/t22_task_capture_failure_cleanup.styio`
- `tests/features/state_resources/t23_task_f64_history_precision.styio`
- `tests/features/state_resources/e06_task_capture_failure_no_fallback.styio`
- `tests/features/task_resources/e06_task_order_non_task.styio`

Read or copy these fixtures into permanent checks; do not hand-copy a reduced
substitute. Derive expectations separately for syntax, supported semantic
diagnostics and runtime outcomes. A syntactically valid failure program is not
a malformed-syntax counterexample. Preserve its compiler/operation failure
contract without requiring semantic diagnostics absent from the existing bridge.

Repair `grammar/tree-sitter-styio/grammar.js` and directly affected
`StyioIDE/Syntax.cpp` tokenization or `TreeSitterBackend.cpp` adaptation at their
owner. Follow `docs/external/for-ide/TREE-SITTER.md` and the IDE/LSP runbook;
regenerate `src/parser.c`, `src/grammar.json` and `src/node-types.json` with
`npx --yes tree-sitter-cli@0.26.8 generate` from the grammar directory, using the
documented Node baseline. Update affected owner documentation and existing test
membership. Do not disable Tree-sitter, suppress valid errors, add a second
grammar/version, substitute an installed binary or special-case a fixture.

Extend existing `tests/ide/styio_ide_test.cpp` and LSP protocol assets instead of
introducing a harness framework. Assert the actual Tree-sitter backend and test
the shared fixtures plus clearly malformed edits. Preserve tolerant recovery,
incremental/full parse equivalence, folding, token ranges and completion.
Independently inspect shared-fixture grammar during review; reuse valid evidence
for unchanged surrounding behavior. Build `styio`, `styio_lspd`, `styio_ide_test`
and other affected test targets from the final source with Tree-sitter enabled.

Real stdio evidence must select `build/default/bin/styio_lspd` explicitly and
exercise initialize/open/change/save for copied good t13, clearly malformed
syntax, then restored good source. Track the document version associated with
each change and save; diagnostics must carry the producing snapshot's version.
Verify current diagnostics clear after restoration and stale publications cannot
become current. Inspect both immediate syntax and merged diagnostics at the
existing request-driven drain boundary in `docs/external/for-ide/LSP.md`; retain
the save notification's actual behavior without inventing a new semantic or
idle-thread contract. Copy programs and use escaped code points for troublesome
generated operators; avoid unrelated full-file rewrites.

Keep full command logs, true process exits, XML and protocol captures in ignored
Styio `build/` evidence directories. Shared summaries contain relative references
and redacted findings only. Each privacy finding requires file/line, rule,
redacted content/category, true-leak or false-positive basis, impact and
disposition. Do not add hashes, source archives, repeated gates or infer passes
from skipped, interrupted or unavailable evidence.

The stopping artifact is the independently reviewed current-source CLI/LSP
candidate. Distinguish retained compiler success, newly demonstrated LSP parity
and the separate Vityo production-service join. The coordinator resumes Vityo;
its Worker owns its independent work and dependent join. Do not replay the
30-step Pafio gate, run full regression, create further execution lanes, commit,
push, open a PR, install, publish, initiate live Agent/installed-UI acceptance or
activate a later milestone.

### U3 Worker candidate and evidence

The repair adds the existing nightly-parser task/history slice to the single
edit-time grammar: resource declarations with bounded type shapes, negative
integer history selectors, task blocks, typed await/fallback, parameterized
stream iteration, resource sink writes, named ordering and print statements.
The tolerant tokenizer now retains `||>`, `..` and `>_` as compiler-matching
tokens. `TreeSitterBackend.cpp` already adapts these nodes and errors correctly;
its diagnostic and incremental-tree paths are unchanged. No compiler execution,
observation, semantic-publication or installed-binary path was replaced.

The owner trace read `parse_styio_type`, `parse_resource_decl_after_at_latest`
and `parse_resource_selector_offset_latest` in `Parser.cpp`, task/await,
iterator/redirect/order and print handling in `NewParserExpr.cpp`, plus
`Token.hpp` and `Tokenizer.cpp`. Feature contracts for slot declaration,
selectors, sink writes, single tasks and await remain the syntax authority.
The grammar follows Tree-sitter's documented structural-rule approach rather
than adding error filtering, an alternative grammar or fixture-name branches.
This is parity for the five listed programs and their affected editor forms,
not universal language coverage. In particular the two failure fixtures remain
syntactically valid; their retained compiler/operation failure evidence is not
reclassified as malformed editor input.

Checked-in parser artifacts were regenerated with
`npx --yes tree-sitter-cli@0.26.8 generate`, exit 0, using Node.js v24.18.1
(the documented macOS 24.x compatible baseline). Final targets are
`build/default/bin/styio`, `styio_lspd` and `styio_ide_test`, built incrementally
with `STYIO_ENABLE_TREE_SITTER=ON`, Debug, in the existing checkout/build.
All evidence below is relative to `build/task-resource-repair-20261011/u3/`.

| Actual execution | Result and evidence boundary |
| --- | --- |
| Before repair: copied t13 through explicitly selected stdio LSP | Server exit 0, parity check exit 1. Good versions 1/3/5 each had 16 syntax errors in both immediate and merged publications. `before/protocol.json`, `before.log`, `before.exit`. This independently reproduces the Vityo report. |
| Pinned parser generation and direct Tree-sitter CLI parsing of all five authoritative files | Both exit 0; `generate.log/.exit`, `grammar-fixtures.log/.exit`. The CLI emitted a parser-directory configuration warning while using the current grammar; actual-backend fixture tests additionally assert Tree-sitter, no error nodes and compiler parsing without recovery. |
| Final CLI/LSP/IDE-test build | `cmake --build build/default --target styio styio_lspd styio_ide_test -j4`, exit 0: `build-candidate.log/.exit`. An earlier test-only build failed on a nonexistent initialization helper (exit 2, `build.log/.exit`); corrected to the existing API. No failed build is treated as passing. |
| Uncovered IDE selections | 34 tests passed, zero failed/skipped: `ide.log`, `ide.xml`, `ide.exit`. Selector covers shared fixtures, syntax, completion, LSP server/runtime, UTF-16 tokens and existing syntax drift. After strengthening fold/error-node assertions, only the six affected fixture/edit cases were rerun: 6 passed, zero failed/skipped, `ide-candidate.log/.xml/.exit`. These executions overlap and are not added into a new total. |
| Final real LSP protocol | Check and server exit 0: `protocol-candidate.log/.exit`, `candidate/protocol.json`, `candidate/server.stderr.log`, `candidate/server.exit`. Good versions 1/3/5 have empty immediate and merged diagnostics. Malformed versions 2/4 have 2 editor errors and 3 merged diagnostics. Saved versions 1/2/3 match their last synchronized snapshots; save itself emits no extra publication. Rapid malformed/repair changes publish versions 4/4/5/5 in order, with no trailing output or stale result after version 5. |
| Permanent registration | `test-membership.log/.exit` lists nine new CTest cases. Listing is registration evidence, not another execution. The real stdio case reuses `tests/lsp_stdio_framing_test.py`; C++ cases reuse `tests/ide/styio_ide_test.cpp`. |

The six malformed edits cover capacity syntax, task block entry, history
selector, await target annotation, iterator parameter delimiter and print
delimiter. Every edit produces actual Tree-sitter error nodes; every restoration
clears diagnostics. Incremental and fresh parses agree on nodes, token spans,
diagnostic ranges, matching delimiters and folds. The handled-failure task's
multiline block retains its fold. Later-binding completion survives a malformed
await. A separate in-process pending-work test retains the old immutable
snapshot and publishes only repaired version 3 after coalescing versions 2/3.
The real protocol evidence checks both publication phases at request barriers;
it does not assume an idle semantic thread or invent save behavior.

Documentation audit passed after updating the required Grammar runbook and
generated IDE index (`docs-audit-final.log/.exit`); the initial failure remains
in `docs-audit.log/.exit`. Changed runbook sizes were refreshed using the existing
document counter. The worktree local-info scan exited 0 with no findings
(`privacy.log/.exit`); subsequent result records and documentation-only additions
were manually reviewed. No per-item privacy findings were produced. Raw local
paths, protocol payloads and stderr stay under ignored build output; shared
records contain relative evidence references only. `git diff --check` exited 0
(`diff-check.log/.exit`). Source revision and dirty paths are retained as ordinary
Git provenance in `source-revision.txt` and `candidate-status.txt`; no new source
archive or content-hash gate was created.

The Worker has not run `styio-task-resource-focused`; that entire registered
check belongs to the host. `styio-task-resource-review` and independent source,
generated-artifact and evidence inspection belong to the subsequent HANDOFF
Reviewer. Prior B1–B5/U1/U2 compiler/Pafio evidence is retained within its recorded
scope; the 30-step producer gate was not replayed. Vityo's production-service
join remains coordinator-owned and is not demonstrated by these local checks.

### U3 independent review and evidence boundaries

The Reviewer independently traced the new grammar productions to the nightly
resource/type/selector, task, await, iterator, sink, named-order and print parser
paths. The generated grammar JSON, node types, parser symbols and productions
agree with this bounded change. The tolerant tokenizer keeps compiler token
boundaries; `Syntax.cpp` still imports Tree-sitter error nodes, and the unchanged
adapter preserves tree edits, missing-node errors and structural folds. The
semantic service retains immutable snapshot/generation/version guards and the
server publishes the producing snapshot's version. No actionable in-scope
production defect was identified in this review; no production or test source
was changed after the Worker candidate build.

The Reviewer inspected the actual Worker build logs/exits, IDE XML, real stdio
transcripts and Vityo's retained 16-error document evidence. The current Debug,
Tree-sitter-enabled `build/default/bin/styio`, `styio_lspd` and `styio_ide_test`
targets are present in the successful final build log. That build and the
unchanged passing IDE/protocol inputs are reused, not rerun for reporting.

| Review evidence | Actual result and limit |
| --- | --- |
| Independent direct grammar verification | `npx --yes tree-sitter-cli@0.26.8 parse --quiet` on all five authoritative fixture paths, exit 0. Exact argv and full log: `build/task-resource-handoff-20261011/u3/grammar-command.json`, `grammar-fixtures.log/.exit`. The CLI configuration warning is retained; no parser error was emitted. |
| Host registered focused check | `styio-task-resource-focused` reports passed through the assigned plan. `build/task-resource-handoff-20261011/u3/host-result.json` records this evidence. Full host console, per-command exits and detailed GoogleTest totals are not exposed by the assigned-plan interface. |
| Retained local CTest evidence from that host execution | `build/task-resource-handoff-20261011/u3/host-focused-ctest.log` starts on 2026-10-11 at 23:19 CST and contains 54 passed / 0 failed records. This is the CTest portion only, not a total for the entire focused entrypoint. |
| Independently inspected LSP capture | Good versions 1/3/5 have empty immediate and merged diagnostics; malformed 2/4 retain two editor errors and an additional compiler parse diagnostic in the merged result. The final saved copy matches authoritative t13. Rapid publications are 4/4/5/5, with no trailing output. Reused capture: `build/task-resource-repair-20261011/u3/candidate/protocol.json`. |
| Recovery and stale-work evidence | Reused Worker XML covers six malformed/restored incremental edits, fresh/incremental equivalence, compiler token ranges, multiline folds, completion after a malformed await and pending-semantic coalescing to version 3. The 6-case strengthened rerun overlaps the 34-case run; totals are not added. |

`didSave` is not an advertised capability or a dedicated handler. The transport
test writes its copied file and sends a save notification after synchronized
versions 1/2/3; the absence of an extra publication proves protocol-state
preservation in that sequence, not implemented LSP save semantics. Actual Vityo
disk-save and production notification wiring remain its document-transaction
obligation. The two failure fixtures are syntax-valid; retained compiler/runtime
failure results remain mandatory and do not imply an additional IDE semantic
diagnostic contract.

The registered reviewer check remains **only** `styio-task-resource-review`.
Its current host execution follows this review and is not asserted by these
records; the older accepted review-check receipt below remains historical.
No whole focused suite, full regression or Pafio producer gate was replayed.
No sibling source/binary, installed application or live Agent was touched.
The stopping artifact is the reviewed current-source CLI/LSP candidate, with
the Vityo join explicitly separate.

The unchanged Worker privacy scan is reused. Reviewer additions contain relative
evidence references and technical findings only; manual review found no new
privacy item requiring a redacted per-item table. Raw protocol/local paths and
retained host logs remain in ignored build output.

## U2 continuation: independently reviewed real-producer compatibility

U2 was reported after B5: Pafio's canonical `namespace/name` was rejected by
both compile-plan admission and `Scope::qualified`. B5 corrected
`root_dir`/`manifest_path`; its hand-built `example.app` payload established
only that earlier field repair, not real-producer compatibility. That historical
evidence is retained below. U2 retains the canonical fields and replaces both
name predicates with `canonical_package_name_is_valid`: O(n) time, O(1) extra
space, explicit ASCII rules matching Pafio `Manifest.cpp::NameRegex`. Each of
the two nonempty segments starts with a lowercase letter or digit; subsequent
characters also permit `_` and `-`; exactly one slash is required.

Independent review read the actual source diff and Pafio's manifest validator,
`BuildPackageJson` and compile-plan schema. The producer forwards the name
unchanged. Review followed admission and unique package matching through
lexical root/manifest/entry containment, `Scope::qualified`, length-prefixed
semantic identity, static publication, snapshot decoding/serialization, delta
parent identity and reconstruction, service scope keys, runtime descriptor
binding and the success-only receipt. This path preserves the name and
package-relative compilation-unit paths; it introduces no alias, fallback or
schema version. Corrected goldens reflect the changed logical name; unrelated
opaque topology fixture IDs retain their values. The new admission test fixes
an opaque package ID independently of all tested names.

| Review finding | Disposition and evidence |
| --- | --- |
| Worker invalid-name coverage exercised only static admission and omitted required grammar classes | Added `PackageNameGrammarForBothObservationModes`: five independent positive and 23 invalid names in each mode, including empty/multiple segments, both segment starts, uppercase, dots, backslashes, whitespace, `@` and non-ASCII. Exact rejection diagnostic and unchanged admitted name/opaque ID/relative paths are asserted. Expanded independent `Scope::qualified` cases. The smaller overlapping name loop was removed from the request matrix; other admission and containment cases remain. |
| Worker gate checked the parent before observed execution | Reused its original snapshot and successful observed runs. Issued only the missing subsequent static check, with that first snapshot as parent, in the same scratch. It published an empty delta with matching parent/target identity; all older files remained byte-identical. Then verified switching observation off after this sequence. |
| Worker summary said 31 steps and 15 unobserved receipts | Retained JSON contains **30** steps, including machine info and doctor, and **14** successful unobserved receipts before review. The two reviewer invocations are separate evidence, not part of that original count. Disabled controller JSONL still exists when observation is off; no static snapshot/delta or task instrumentation is emitted. |
| Exact Worker build exits were not retained beside test logs | Do not reconstruct process exits from passing GoogleTest text. Reuse the available per-binary test output and host result with that limit; the reviewer recorded an exact incremental final-target build and true exit below. |

The actual producer is the existing Pafio binary in the sibling workspace
checkout (its host path is not material here; treat it as `<workspace-root>`),
consumed in place, with base revision `a20dce6e9d889e892457b45b14d8d745e686ed34`
plus its existing dirty candidate. The revision alone does not identify the
binary. The gate source copies Pafio's authoritative manifest/programs into
`build/task-resource-repair-20261011/pafio-gate/project/` and uses an isolated
scratch HOME under that gate directory. Review retained byte-identical copies of all five sources,
five expectations and the manifest in `pafio-gate/authoritative-fixtures/`.
There is one scratch project. Every real request explicitly selects
`build/default/bin/styio`; the deliberately unusable `PAFIO_STYIO_BIN` proves
that selection wins. No sibling source/binary was changed or rebuilt. The
Worker used the owner Python harness with its work directory under Styio;
review used direct Pafio CLI on that scratch. No sibling build/check entrypoint
was invoked by this review.

Review independently read all current receipt-listed files and their plans,
verified package matching and `acme/task-resource` / `pafio.toml` /
`src/main.styio` identity, and compared all three successful observed runs
against the copied t13 output expectation. Runtime session framing, per-task
creation/completion/result-consumption, absence of task failure, and event
snapshot/site IDs matching the original static snapshot were checked. The
retained missing-parent case records `full_snapshot_required/parent_unreadable`;
it is additional degradation evidence, not a substitute for the valid-parent
empty delta. Initial, failure and B1–B5 artifacts remain retained.

Reviewer evidence below is under
`build/task-resource-handoff-20261011/canonical-name/`; Worker logs remain under
`build/task-resource-repair-20261011/logs/`. Raw paths, sessions, commands,
stdout/stderr and artifacts are ignored and are not shared in these summaries.

| Execution/evidence | Actual result and reuse boundary |
| --- | --- |
| Host `styio-task-resource-focused` | Assigned plan reports passed. Full console, per-command exits and GoogleTest totals are not exposed. Separate retained CTest log contains 54 passed records, zero failed; no aggregate total is inferred. `host-focused-evidence.json`, `host-focused-ctest.log`. |
| Worker snapshot producer / static consumer / observable consumer / delta / query / service | Retained logs report 28 / 8 / 3 / 11 / 4 / 5 passes respectively; no skips reported. No separate exit files retained. Unchanged cases reused; altered request matrix superseded below. These are separate executions. |
| Worker topology / lowering / type inference / constrained runtime / main contract | Retained logs report 57 / 26 / 45 / 25 / 45 passes respectively; no skips reported. Compiler selection reports 140 passes and two platform skips, which are not passes. No separate exit files retained. |
| Reviewer `cmake --build build/default --target styio styio_lspd styio_observable_static_snapshot_test styio_resource_topology_test -j4` | Exit 0. Existing macOS arm64 Debug configuration; both CLI and LSP targets checked by incremental dependency tracking; changed test executables rebuilt. `build-command.json`, `build.log`, `build.exit`. |
| Reviewer new dual-mode grammar and altered request matrix | 2 tests passed, zero failures/skips, exit 0. `name-admission.xml/.log/.exit`; exact filter in `test-commands.json`. |
| Reviewer expanded semantic identity grammar | 1 test passed, zero failures/skips, exit 0. `semantic-name.xml/.log/.exit`; exact filter in `test-commands.json`. |
| Worker actual producer harness | Retained `pafio-gate.json` records 30 steps with expected actual exits (0 or the intentional compiler-failure 23), `ok=true`, no validation errors. Harness exit 0 is Worker-reported; no separate exit file retained. This review did not replay that gate. |
| Reviewer direct `pafio --json check --frozen --emit-observable-static-snapshot --observable-parent-snapshot <first-snapshot>` with explicit scratch manifest/compiler | Exit 0. Issued after the retained observed runs; receipt owns matching snapshot and empty delta. `parent-after-runtime-command.json`, `.stdout/.stderr/.exit`. |
| Reviewer direct `pafio run --frozen` with the same explicit manifest/compiler and isolated home | Exit 0. Expected output, disabled controller, no task events/snapshot/delta; previous artifacts preserved. `observation-off-after-runtime-command.json`, `.stdout/.stderr/.exit`. |
| Reviewer artifact verification | Exit 0; `review_producer.py`, `producer-review.json/.log/.exit`. Checks retained Worker output plus the two new requests, preserving 137 earlier files by the final comparison. Initial verification stopped before any producer call because it inherited the incorrect receipt count; that exit 1/log is retained as `producer-review-initial.*`, then the count was corrected. No successful producer request was replayed. |
| One-off retired scoped usage search | Exit 1 means no matches, not a failed compiler/test. `retired-usage-command.json`, `.log/.exit`; no permanent migration gate added. |
| Reviewer documentation audit and worktree local-info scan | Both exit 0; `docs-audit.log/.exit`, `privacy.log/.exit`. No local-info findings. Later result-record edits manually reviewed; neither scan was replayed. |

Only tests/evidence/docs changed during review; the Worker production repair
stands. Unchanged B1–B5 evidence and valid producer runs are reused. The whole
focused entrypoint and full regression were not rerun. The only host review
selection is `styio-task-resource-review` (patch whitespace); its execution
follows this review and is not claimed here. These tests are bounded evidence,
not a zero-defect assertion.

The reviewed in-place Styio CLI/LSP candidate is the handoff. The coordinator
alone resumes Pafio's paused admission/check and admits Vityo's production
service/daemon join using the same candidates. This local producer evidence
does not establish that downstream join, Task/Tree delivery, another platform,
full regression or live Agent/UI acceptance.

## Historical B5 continuation: canonical compile-plan package fields

Pafio's `contracts/compile-plan/v1/compile-plan.schema.json` package definition
and `src/PafioPlan/CompilePlan.cpp` `BuildPackageJson` own the producer fields
`root_dir` and `manifest_path`. Independent review confirmed that Styio now
reads those fields at `CompilePlanContract.cpp:599–601`, including qualified
missing/type/absolute-path and manifest-containment diagnostics. The prior
consumer incorrectly demanded `root` and `manifest`.

The reported real Pafio invocation exited 23 with
`STYIO_SERVICE_COMPILE_PLAN_INVALID` / `observable_static_snapshot_anonymous`.
That is coordinator-supplied defect evidence, not a Pafio invocation run by
this reviewer. The Worker's separate canonical-payload Styio reproduction
exited 6 before repair and 0 after repair. Its diagnostic, exit files, snapshot
and receipt remain under
`build/task-resource-repair-20261011/package-identity/`. Review inspected the
snapshot's relative compilation-unit identity and receipt structure without
exporting raw paths or session values.

Review found incomplete positive test producers: the common snapshot CLI
helper and runtime-only CLI case omitted required package metadata and used
the wrong resolver literal. They now provide the v1 package record and
`single-version-v1`. The new admission regression independently mutates each
identity field, covering missing canonical keys, wrong type, empty/relative
paths and an escaping manifest for both static snapshot and runtime-only
observation. Both accepted modes assert the same package/manifest/entry
identity. Canonical unobserved and intentionally minimal unobserved plans
remain accepted. Positive cases were compared with the producer schema by
source review; no external schema-validation tool was run.

The review changed tests and the Pafio-facing contract explanation; it made
no additional production change. CLI and LSP artifacts are still selected at
`build/default/bin/styio` and `build/default/bin/styio_lspd`, in the same
macOS arm64 Debug configuration and original branch/build directory. The
Worker built all current focused targets plus the snapshot producer/consumer
tests. The reviewer incrementally built `styio`, `styio_lspd`, `styio_test`
and `styio_observable_static_snapshot_test` after the test corrections, exit 0.
No sibling binary was rebuilt or replaced.

| Execution or evidence | Actual result and reuse boundary |
| --- | --- |
| Host `styio-task-resource-focused` | Host plan reports passed after the Worker. Its full console, per-command exits and GoogleTest totals are not exposed by the assigned plan API; no such details are inferred. The retained CTest log independently records 54 passed state/task/LSP tests. |
| Worker `styio_observable_static_snapshot_test` | 28 passed, no failures/skips, in `snapshot-test.xml`; original execution retained. The changed helper/admission cases are superseded by affected reviewer evidence below; unchanged producer/identity/machine-info cases are reused. |
| Reviewer affected build | Exit 0; `affected-build.log`, `.exit` and `affected-build-command.json`. |
| Reviewer snapshot admission, affected CLI and delta publication selections | 17 passed, no failures/skips, exit 0; `snapshot-affected.xml`, `.log`, `.exit`. Includes relocation-stable bytes, unobserved operation, pre-parse rejection, publication failure and receipt/delta ownership. |
| Reviewer `StyioDiagnostics.CompilePlanNegotiatesRuntimeEventsV2` | 1 passed, no failures/skips, exit 0; `runtime-negotiation.xml`, `.log`, `.exit`. |
| Reviewer documentation audit and worktree local-info scan | Both exit 0; `docs-audit.log`, `privacy.log` and corresponding `.exit` files. No local-info findings; table below records the disposition. |

Reviewer evidence in this table is relative to
`build/task-resource-handoff-20261011/package-identity/`; exact selectors are
in `test-commands.json`. `host-focused-evidence.json` records the host evidence
boundary and `host-focused-ctest.log` preserves the existing CTest output.
The complete focused entrypoint and full regression were not rerun by this
reviewer. Counts describe separate executions and must not be added together.
The registered HANDOFF selection remains only `styio-task-resource-review`;
its current host execution follows this review and is not claimed here.

Source review followed admission into static publication and runtime binding.
Both use `CompilationUnit` with canonical relative paths; no absolute root is
added to semantic identity. Receipt serialization remains Styio-owned and
opaque to Pafio. No alias, fallback reader or new schema version was added.
The snapshot document's independent `root`, its relative
`compilation_unit.manifest_path`, nano manifests and native build manifests
retain their contracts. A one-off scoped search found no superseded package
field access in production; negative tests deliberately retain obsolete key
strings. No migration-only gate was added.

This candidate is ready for Pafio to validate observation against the selected
CLI. That operation and the same-candidate cross-repository join remain the
coordinator's responsibility; readiness does not admit Vityo or establish the
full milestone. The earlier cost/ownership/scheduling evidence below is reused
for unchanged implementation, not represented as re-executed in this continuation.

## Earlier handoff candidate and build inputs

- Base revision: `09f8fbdf6af9e27e590def6731eaa660d595a36c`; reused branch
  `agent/downstream-styio-docs-20261005` and `build/default`.
- The candidate includes uncommitted implementation, contract, test and planning
  changes. The revision alone does not identify it. Relative changed-path lists,
  the tracked patch, untracked input copies and build configuration are recorded
  under `build/task-resource-handoff-20261011/` in `provenance.json` and
  `candidate-inputs/`. No source tree snapshot replaced the existing checkout.
- Existing `docs/INDEX.md`, `docs/plan/INDEX.md`, `docs/plan/README.md` edits were
  preserved. Existing programme/task/tree definitions were reused. No commit,
  push, PR, installation, publication or sibling-repository edit was performed.
- Earlier handoff incremental command (superseded for B5 by the builds above):
  `cmake --build build/default --target styio styio_lspd styio_test styio_resource_topology_test styio_main_contract_test styio_observable_runtime_test styio_codegen_internal_test -j4`
  exited 0. Evidence: `final-targets-build.log` in the handoff build directory.
  This explicitly rebuilds the compiler **and LSP daemon** from the final source;
  the baseline daemon is not substituted for the handoff artifact.
- Actual configuration: macOS arm64, Debug, Unix Makefiles, Apple Clang 21,
  LLVM 18, CMake 4.3.1, Tree-sitter ON, ICU OFF, Nano ON. Other platforms,
  Release, sanitizers and alternate feature configurations were not verified.

| Available artifact | Use and evidence |
| --- | --- |
| `build/default/bin/styio` | Selected source-built CLI/JIT/native builder; final language/contract evidence below |
| `build/default/bin/styio_lspd` | Selected source-built language daemon; final stdio framing check passed |
| `build/default/bin/styio_test`, `styio_resource_topology_test`, `styio_main_contract_test`, `styio_observable_runtime_test`, `styio_codegen_internal_test` | Built engineering executables; only the explicitly listed test selections were run |
| `build/task-resource-handoff-20261011/machine-info.json` | Fresh identity/capability response from the selected CLI |

Machine info reports `0.0.1 / nightly / full`, machine-info/JSONL diagnostics/
syntax-check/compile-plan/receipt contract v1 and runtime-events v2. These are
advertised capabilities, not proof that every feature was tested. The baseline
inventory contains 46 feature contracts; its 44 `converged` labels and configured
CTest inventory are not acceptance evidence. Runtime cancellation, cooperative
suspension and I/O/resource/timer waits remain explicitly unavailable. Native
matrix capture, native reverse task order and LSP framing were actually checked;
complete IDE behavior and other platforms remain outside this evidence.

## Defect ledger and independent source review

Review covered every changed implementation surface and the affected callers:
semantic analysis and resource topology; lowering and held-task selection; IR
walker/verifier/type/printer; task and ring LLVM emission; scheduler, resource
ownership and observation; compile-plan admission/output production; CMake test
membership; current feature and ecosystem contracts. Patch hygiene alone was
not used to establish these findings.

| Finding | Final source behavior | Permanent evidence |
| --- | --- | --- |
| B1: captured history disappeared | Referenced committed rings and heads are captured at creation. Numeric arrays retain two independent stores with bulk copies; strings and container graphs have explicit cross-thread ownership. | State `t13`, `t14`, `t17`, `t19`–`t23`; task `t05`, `t11`; runtime `TaskCapture*` |
| B2: child committed/overwrote parent pending writes | Lazy per-scope pending arrays replace shared-buffer count markers. Child exit commits only child writes; parent storage is restored. Capacity-1 wraparound now preserves parent 9. | State `t15`, `t16`, `t18`; existing pulse/stream tests |
| B3: order edge disappeared | Task type checks, `SIOTaskOrder`, held after-targets and runtime edges enforce direction, including `second => first`. Captures still occur when tasks are created. | Task `t07`–`t10`, native `t10`, `e06`–`e10`; runtime `TaskOrder*` |
| Runtime graph/lifetime/backpressure | Graph registration and launch are synchronized. Executor ownership survives readiness publication and successor dispatch. Demand launches held ancestors; full worker queues use work-first execution. | General-cycle settlement and capacity-2 fan-out/release tests |
| General language cycle allegation | The CLI already rejected two-/three-node cycles through semantic resource topology; no language hang reproduced. Direct runtime ABI lacked the same protection and now rejects reachability cycles. | Task `e08`, `e09`; runtime general-cycle test |
| Observation orphan WaitEnd | Wait IDs belong to an observation session epoch and are consumed at end. Prior-session queue waits cannot produce unmatched events in a new session; atomic test clock removes a test race. | Whole observable module and the strict session-order test |
| B4: old receipt/diagnostics survived | Prior receipt invalidation precedes diagnostic setup; diagnostics are invocation-local; only success writes a receipt. Directory failure and early recoverable admission failures cannot preserve an old success. | Repeated-invocation and directory-setup contract tests |
| C1 f64/identity/outer plan | No precision defect reproduced; identity and qualified plan-field diagnostics are preserved. | Machine-info/compile-plan test group, state `t23`, main contract cases |

`CodeGenIO.cpp` exports owned capture packets, then imports them into the worker
handle table. Nested container tests destroy the parent graph before use and
check worker cleanup after failure. Ordinary computed string captures survive a
parent stage exit. Returned strings are copied before task-local cleanup, then
freed by the runtime after result ownership transfers. Invalid direct-ABI cycles
are diagnosed; cleanup still settles held tasks. No arbitrary timeout was added.

Existing alias boundaries were inspected: explicit clone/value behavior is
supported; resource-sharing aliases and block-local resource declarations are
not supported syntax. The new cases preserve those boundaries instead of adding
new syntax. No multiwriter merge, external-I/O rollback, cancellation or new
language contract is introduced. There is no unresolved contract decision in
this finite repair ledger.

Runtime cycle validation uses an explicit work stack and visited set, as in
[Boost's iterative DFS implementation](https://github.com/boostorg/graph/blob/develop/include/boost/graph/depth_first_search.hpp),
with O(V+E) reachable-graph work per edge check. It maintains live predecessor/
successor adjacency and removes settled edges; it does not rebuild or persist a
second full topology. Numeric copies use the existing LLVM memory intrinsic;
[LLVM's contract](https://llvm.org/docs/LangRef.html#llvm-memcpy-intrinsic) permits
this for the distinct context and local arrays. Owning elements retain their
export/import operations. See [COST.md](COST.md) for actual measurements and
retained/rejected choices.

## Earlier handoff engineering verification

All evidence paths below are relative to
`build/task-resource-handoff-20261011/`. These are separate executions, often
with overlapping cases; no task-wide aggregate is claimed. Successful results
were reused when their inputs remained applicable.

| Command/selection | Actual outcome and scope | Evidence |
| --- | --- | --- |
| Final target build listed above | Exit 0; compiler, daemon and all required test executables | `final-targets-build.log` |
| `ctest --test-dir build/default --output-on-failure --no-tests=error -R '^(state_resources_\|task_resources_\|styio_lspd_stdio_framing$)'` | 54 passed: 31 state, 22 task (including two native build/run cases), 1 LSP framing | `final-language-tests.log` |
| `styio_codegen_internal_test` filter `StyioCodeGenInternal.Task*:StyioCodeGenInternal.CallableBlockTailSkipsCStringEscapeOwnership:StyioCodegenInternal.*TaskAbi*` | 7 passed after the final scalar-capture and bulk-copy edits; includes capacity-8192 IR-size regression | `final-codegen.xml`, `.log` |
| `styio_test` filter `StyioResourceTopology.*:StyioResourceEffects.*:StyioStreamZip.*:StyioDiagnostics.MachineInfo*:StyioDiagnostics.CompilePlan*` | 142 selected: 140 passed, 2 platform skips. Reused for unchanged pulse, effects, diagnostic and contract paths; final capture changes checked above. | `compiler-focused.xml`, `.log` |
| `styio_resource_topology_test` | 57 passed; topology implementation unchanged after this run | `topology.xml`, `.log` |
| `styio_main_contract_test` | 45 selected: 44 passed, 1 failed on the changed directory-error diagnostic. Fixed by retaining the existing diagnostic for ENOTDIR; the failed case and receipt-freshness case then both passed. The 44 unaffected passes are reused. | `main-contract.xml`, `c1-corrected.xml` and corresponding logs |
| `STYIO_TASK_QUEUE_CAPACITY=2 STYIO_TASK_THREADS=1 styio_observable_runtime_test` | 24 passed; then the additional nested-container failure/ownership case passed separately. Runtime production sources unchanged since these runs. | `observation.xml`, `nested-ownership.xml` and corresponding logs |
| Earlier focused CTest execution | 48 passed, 2 test-fixture mismatches (integer-literal order grammar; incomplete division diagnostic expectation). Corrected both fixtures and passed both; final 54-case run above supersedes this language evidence. | `focused.log`, `fixture-corrections.log` |
| Cost comparisons | i64 expected checksums matched in JIT and native execution for six selective/full-snapshot cells and the bulk-copy comparison; see COST.md | `cost/results.json`, `cost/bulk-results.json` |
| `python3 scripts/docs-audit.py` | Initial failure: report metadata, generated indexes and required module runbooks were missing updates. Corrected those scoped documents and the rerun passed. | `docs-audit.log`, `docs-audit-corrected.log` |
| `python3 scripts/local-info-leak-gate.py --mode worktree` | Exit 0, no findings; new files included | `privacy-final.log` |
| Registered `bash docs/plans/task-resource-delivery/checks.sh review` | Exit 0; tracked-patch whitespace only, not semantic review | `review.log` |

The two skips are
`StyioResourceEffects.NamedCleanupHandlerRunsForFileCloseFailure` and
`StyioResourceEffects.IoHandlerDoesNotCatchFileCloseCleanupFailure`; neither is
reported as passed. Initial negative reproductions (orphan WaitEnd, capacity-1
pending overwrite and invalid worker matrix handle) remain in the ignored build
directory as defect evidence, not acceptance results.

The expanded `checks.sh focused` now addresses permanent state/task fixtures,
LSP framing, relevant compiler/contract/codegen tests and the observable runtime
module. It was not rerun wholesale merely to populate this handoff: actual
component commands and affected reruns are listed above. The only registered
HANDOFF check remains exactly `styio-task-resource-review`.

## Requirement mapping and downstream conditions

| Requirement | Evidence or remaining owner |
| --- | --- |
| REQ-STYIO-BASELINE | Current-source final target build, dirty input record, machine info, scoped platform inventory |
| REQ-STYIO-REPAIR / SNAPSHOT / ORDER | Defect ledger, production wiring, permanent CLI/native and runtime cases above |
| REQ-STYIO-RESEARCH | Correct full-snapshot comparisons, bulk-copy experiment and exact context/IR evidence in COST.md |
| REQ-STYIO-BOUNDARY | Existing checkout and source assets retained; no later milestone or live workflow invoked |
| REQ-STYIO-INTEGRITY / REGRESSION | Scoped affected checks passed as stated; complete engineering regression and integrated usability are not established here |
| REQ-STYIO-E2E | Coordinator-owned same-candidate Pafio/Vityo production-path evidence remains required |

The compiler is selected in place with its existing build/runtime inputs retained;
standalone relocation of the executable was not tested. The coordinator can
select the relative CLI and daemon artifacts above now;
HANDOFF does not wait for Pafio or this Tree. Configure external binary paths
outside source control. Pafio must probe this actual CLI identity/capabilities,
consume compile-plan/diagnostic/receipt output for the current invocation, and
exercise success → compile failure → repaired success plus f64 and task
settlement. Malformed plans without recoverable output roots require caller
invalidation; concurrent invocations must not share output paths.

Vityo then joins the same compiler/Pafio candidates through actual production
services and vityod, including stale-result isolation and the required LSP/IDE
path. Cross-repository prerequisites are coordinator-managed, not scheduler
edges. Only after common same-candidate E2E and all three source reviews may the
coordinator admit final complete regression. Existing NODE-FINAL/NODE-CLOSE
remain pending; no such automation nodes were created or executed. Task/Tree
delivery fields and future outlines were not advanced.

## Privacy and evidence handling

Raw logs, generated plans, runtime captures, local toolchain paths and benchmark
binaries remain under ignored `build/`; they are not copied to the blackboard.
The blackboard and this handoff contain safe source/result summaries and relative
artifact references. `python3 scripts/local-info-leak-gate.py --mode worktree` exited 0. It scanned
tracked and unignored files (including new planning/fixture files); build logs
were excluded by Git ignore rules. This is a local-info rule scan plus manual
review of changed text, not a universal secret/security proof.

The B5 continuation ran the document audit and worktree local-info scan once
after its test/contract/HANDOFF edits; both exited 0. Evidence is in
`build/task-resource-handoff-20261011/package-identity/`. Subsequent edits only
record those results and were manually checked; the earlier scan is not
described as re-executed. Git ignore membership for the evidence directory was
confirmed. No raw evidence was copied to shared records.

| File/line | Rule | Redacted content/category | Judgment and basis | Impact and disposition |
| --- | --- | --- | --- | --- |
| No finding | Local-info path/address rules | No matching content | Scanner returned no findings; changed text review found no private runtime payload or credential additions | No remediation needed; raw evidence remains ignored |
| B5 changed files; no matching line | Local-info path/address rules and manual credential/runtime-data review | Test paths are constructed temporary inputs; shared records use relative artifact references only | Continuation scan returned no findings; final result-record additions manually reviewed | No disclosure found in the reviewed changes; raw plan/receipt/session data stays ignored |
| U2 changed sources, tests and shared evidence documents; no matching line | Local-info path/address rules; manual credential/runtime-data review | No matched private path, credential or runtime payload; test inputs are synthetic | Worktree scan exit 0 and changed-text inspection found no disclosure | No detected disclosure in that scope; full producer/runtime evidence remains ignored and shared summaries use relative references. |
| CLOSE-reviewed changed tracked/unignored text; no matching line | Repository local-info path/address rules; manual credential/runtime-data review | No matched sensitive category in 158 files | `close/privacy.json` records no findings; added test uses synthetic task values and shared records use relative references | No detected disclosure within the reviewed scope; no content remediation needed. Raw evidence remains ignored; this is not universal secret detection. |

## Coordinator host-check acceptance (2026-10-11)

The host subsequently completed the registered handoff check with
`validation=passed`.
The coordinator accepts the current reviewed dirty-source candidate for the
next dependent phase. Full host console and individual process exits are not
exposed; no additional per-case totals are inferred. Existing detailed local
evidence retains its stated scope. Vityo production integration and final
regression remain outstanding; source Task/Tree delivery is not complete.

## Coordinator join and delivery closure — 2026-10-15

The coordinator joined the three current-candidate results after the external
join was completed. Candidate revisions: styio `ba37f64`, pafio `62636ea`,
vityo `cb8883de`; all three worktrees clean.

- Same-candidate Vityo E2E through real production services and vityod:
  35 required real cases and 4 deterministic contract cases
  accepted, `ok=true`, failure category null, not skipped. Inputs are the
  current styio CLI, `styio_lspd`, the current Pafio binary (bounded
  per-invocation retention) and vityod (zero-deadline fix), recorded in
  `inputs.json`. This run replaces the earlier byte-identical reuse claim, so
  the relinked CLI identity is covered by an actual run.
- Expanded Pafio joined integration gate on the same binaries: `ok=true`,
  `validation_errors` empty.
- vityod crate tests: 56 passed, 0 failed. The bounded-retention probe on the
  current Pafio binary confirmed the documented bound (newest 8 plus the
  current invocation plus a referenced parent-snapshot owner).
- Styio production source is unchanged since the accepted FINAL result.
- The registered CLOSE review check was executed as host during this closure;
  NODE-FINAL and NODE-CLOSE are completed and the review items invalidated by
  NODE-REPAIR's result change were resolved against the joined evidence.
- Source `TASK-001` and Tree delivery are recorded; the hosted execution is closed.

Boundaries are unchanged: macOS arm64 Debug only; Linux/Windows, Release,
sanitizers, installed-client UI, real Agent/provider conversations and
unadvertised `didSave` semantics remain unverified. Landing and publication
are separate steps.
