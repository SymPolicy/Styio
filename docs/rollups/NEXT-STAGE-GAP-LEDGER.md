# Next-Stage Gap Ledger

**Purpose:** Provide the active, evidence-based phase summary for repository-wide unfinished work so maintainers can split the next stage into checkpoint-sized, multi-team deliveries without creating parallel truths.

**Last updated:** 2026-10-01

**Status:** Active collaboration ledger. This file distinguishes:

1. what the `styio` repository has already delivered,
2. what is still missing inside `styio`,
3. what is intentionally owned by `pafio-nightly` or another adjacent repository.

## 1. How to Use This Ledger

1. Start from [CURRENT-STATE.md](./CURRENT-STATE.md), then use this file to decide next-stage ownership and sequencing.
2. Treat each gap below as a coordination item, not as a free-form backlog note. Every implementation checkpoint should point back to one or more ledger items.
3. When a gap changes status, update this file, the owning runbook, and the relevant SSOT or handoff document in the same checkpoint.
4. Do not use this file to redefine language semantics, package-manager product scope, or IDE public behavior. Those still belong to the owning SSOTs.

## 2. Current Baseline That Is Real

1. Project priorities and decision order remain fixed by [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).
2. Accepted behavior is owned by the distributed feature contracts and current `tests/features/` registrations in the test catalog. Retired milestone labels are not an independent acceptance authority.
3. The repository is nightly-first rather than legacy-first for parser/toolchain execution.
4. `styio` already provides a bootstrap nano package producer/verifier contract, including `--nano-create`, `--nano-publish`, static repository materialization, and `--machine-info=json`.
5. The IDE stack is real and usable for completion, hover, definition, references, symbols, semantic tokens, incremental edits, and semantic query caching, but it is not feature-complete or operationally closed yet.

## 3. Executive Summary

| Stream | Current reality | Next-stage pressure |
|--------|-----------------|---------------------|
| Frontend / Parser | Nightly parser is active but still a subset with explicit unsupported continuations and fallback paths | Close subset gaps before more behavior migrates onto nightly-only assumptions |
| Sema / IR | Fabricated-zero lowering is closed; intentional/parent-owned visitors and remaining implementation-debt families are classified in IM-D1 | Select one accepted debt family and preserve typed rejection for unsupported forms |
| Codegen / Runtime | Multi-stream zip and stream-driver combinations are only partially lowered | M7 remains incomplete end-to-end |
| CLI / Nano | Bootstrap nano contract exists; full package lifecycle does not | Keep `styio` limited to compiler contracts and handoff surfaces |
| IDE / LSP | Budgeted request-loop diagnostic drain and background-work fallback are implemented; rename, codeAction and inlayHint have conservative evidence-gated implementations | Preserve drain/identity tests and scope any method expansion through the IDE contract |
| Tests / Quality | Feature-based resource-topology, state, stream and negative-syntax tests are registered; remaining contract and release evidence is tracked per surface | Coverage closure must run with the corresponding implementation slice |

## 4. Responsibility Split: Styio vs Pafio

| Area | Owning repo | Status in `styio` | Notes |
|------|-------------|-------------------|-------|
| Nano package materialization, static local registry consume/publish, compiler capability reporting | `styio` | Delivered baseline | See [../external/for-pafio/Styio-Nano-Pafio-Coordination.md](../external/for-pafio/Styio-Nano-Pafio-Coordination.md) |
| Project workflow UX, dependency resolution, lockfiles, vendor, pack, and publish client | `pafio-nightly` | Out of scope here | See [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md) |
| `pafio build/check/run/test` live compile-plan handoff | Pafio producer, Styio consumer | Delivered baseline in `styio` | `--machine-info=json` advertises `compile_plan:[1]`; compile-plan consumption materializes artifacts, receipt, diagnostics, and runtime events |
| Remote registry service semantics, auth/signing/trust, hosted workspace, and cloud worker lifecycle | Styio Platform | Not owned here | Styio owns only compiler-local outputs and capability contracts |

## 5. Detailed Gap Ledger

### 5.1 Frontend / Parser

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Nightly expression parser remains a constrained subset | High | Unsupported continuation guard and explicit rejections such as dot-chain-after-call still exist in [src/StyioParser/NewParserExpr.cpp](../../src/StyioParser/NewParserExpr.cpp) | Frontend, Test Quality | Convert unsupported paths into explicit checkpoint queue; each removed rejection must ship with parity tests and shadow gate evidence |
| Resource selectors and driver coverage remain partial | Medium | Current declarations pass parser/Sema/lowering and bounded snapshot tests; unsupported shapes such as unbounded matrix snapshots remain rejected in [resource_topology_test.cpp](../../tests/resource_topology_test.cpp) and [state-resource negative fixtures](../../tests/features/state_resources/e06_selector_snapshot_unbounded_matrix_unsupported.styio) | Frontend, Sema / IR, Codegen / Runtime | Select a concrete remaining slice from [Resource Topology](../design/Styio-Resource-Topology.md); preserve supported behavior and explicit rejection evidence |
| Handle / capability / failure-type unification is still target design rather than active compiler behavior | Medium | Design doc explicitly says the model is not fully implemented and keeps remaining capability/protocol special cases in [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md); typed stdin ingestion is no longer evidence of an active AST split | Frontend, Sema / IR, Codegen / Runtime | Decide whether next stage closes this model or deliberately continues with local special-case patches |

### 5.2 Sema / IR

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Direct placeholder lowering | Closed | The P0 audit found no active AST route that fabricates `SGConstInt(0)`; the two remaining zero constructors initialize concrete resource storage in [src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) | Sema / IR, Test Quality | Preserve typed failure for unsupported AST families and explicit `SGNoOp` for intentional no-op forms |
| Empty Sema visitor classification | Closed for P0 | All 41 empty visitors are classified in [IM-D1-STYIOIR-CONTRACT-INVENTORY.md](./IM-D1-STYIOIR-CONTRACT-INVENTORY.md): 26 intentional or parent-owned, 5 retired, and 10 implementation-debt families; `FmtStrAST` is confirmed non-empty | Sema / IR, Test Quality | Re-open only one accepted implementation-debt family at a time |
| Inline clone unsupported-node fallthrough | Typed boundary | `StateExprCloneVisitor` explicitly clones the accepted state/resource-method surface and raises `StyioTypeError` for unlisted AST kinds; focused clone tests cover accepted and rejected paths | Sema / IR, Test Quality | Extend the switch only with the implementation slice that accepts the corresponding source form |
| Loop-control verifier context | Closed | [src/StyioIR/Verifier.cpp](../../src/StyioIR/Verifier.cpp) tracks body-scoped nesting for all three loop IR nodes and rejects top-level `SGBreak` / `SGContinue`; focused contract coverage lives in [tests/security/styio_security_test.cpp](../../tests/security/styio_security_test.cpp) | Sema / IR, Test Quality | Preserve the single-level control contract and reopen only for an independently approved semantic extension |

### 5.3 Codegen / Runtime

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Zip lowering still supports only a narrow source set | High | Unsupported source combinations still throw in [src/StyioCodeGen/CodeGenG.cpp](../../src/StyioCodeGen/CodeGenG.cpp) | Codegen / Runtime, Sema / IR, Test Quality | Expand supported combinations according to the owning stream feature contracts, not ad hoc one-off cases |
| Some accepted runtime-oriented syntax still depends on special-case routing rather than a unified protocol | Medium | This is reflected both in current parser/analyzer shape and in the still-target-only capability design [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md) | Codegen / Runtime, Sema / IR | Use next-stage runtime work to reduce parser-shape-driven behavior branching |

### 5.4 CLI / Nano / `pafio` Handoff

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| `compile_plan` live handoff baseline exists, but contract hardening is still narrower than a full release matrix | Medium | `--machine-info=json` reports `compile_plan:[1]`; compile-plan success paths write artifacts, receipt, diagnostics, and runtime events, while invalid producer and malformed request paths return machine-readable failures in [src/main.cpp](../../src/main.cpp) and [tests/styio_test.cpp](../../tests/styio_test.cpp) | CLI / Nano, Docs / Ecosystem, Pafio coordination | Keep the current v1 boundary stable while expanding malformed-input coverage without pulling project/package lifecycle into Styio |
| Full project and package workflow is not implemented here by design | High | The repository boundary assigns project workflows, dependency resolution, locks, vendor, pack, and publish client behavior to Pafio in [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md) | CLI / Nano, Docs / Ecosystem | Preserve this boundary; do not let the compiler CLI accumulate Pafio responsibilities |
| Remote publish protocol is still intentionally absent | Medium | `--nano-publish` rejects HTTP(S) roots and only accepts local path or `file://` repository roots in [src/main.cpp](../../src/main.cpp) | CLI / Nano, Pafio coordination | Keep compiler bootstrap publication local/static; remote registry service behavior belongs to Styio Platform and client behavior belongs to Pafio |
| Edge-path nano validation lacks the same depth as the happy path | Medium | Happy-path bundle/create/publish tests exist in [tests/styio_test.cpp](../../tests/styio_test.cpp), but guard/error branches remain mostly code-only in [src/main.cpp](../../src/main.cpp) | CLI / Nano, Test Quality | Add explicit negative-path tests for marker parsing, blob integrity mismatch, and mutually-exclusive CLI guards |

### 5.5 IDE / LSP

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| LSP surface is still intentionally incomplete | Medium | Current limits retain local-only, single-workspace behavior; [Server.cpp](../../src/StyioServices/StyioLSP/Server.cpp) advertises and handles conservative rename, diagnostic-backed code actions and call-argument hints, with freshness and identity guards documented in [LSP.md](../external/for-ide/LSP.md) | IDE / LSP, Docs / Ecosystem | Expand the surface only after runtime drain and semantic identity paths remain stable under tests |
| Perf budget enforcement is split between unit and dedicated Release harnesses | Low | `StyioIdePerf.EnforcesFrozenLatencyBudgets` skips non-Release runs in [tests/ide/styio_ide_test.cpp](../../tests/ide/styio_ide_test.cpp), with operational guidance in [../teams/PERF-STABILITY-RUNBOOK.md](../teams/PERF-STABILITY-RUNBOOK.md) | IDE / LSP, Perf / Stability | Preserve the dedicated Release gate, but keep the distinction visible so teams do not mistake Debug green for perf closure |

### 5.6 Tests / Quality / Perf

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Next-stage migration tests need to stay tied to active syntax | Medium | [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md) indexes current state-resource and topology feature coverage and keeps retired state-family spellings only as negative migration diagnostics | Test Quality, Frontend, Sema / IR | Add new positive coverage only for active syntax; keep retired spellings in negative tests with stable migration diagnostics |
| Package and contract negative-path testing still lags behind implementation branches | Medium | Nano create/publish guards, marker parsing, and blob verification are present in code but not closed by matching test density | Test Quality, CLI / Nano | Treat contract-edge coverage as release-blocking for any future nano handoff changes |
| Broadened ignore baselines can still hide future tracked repro fixtures outside the frozen negate roots | Low | Root ignore rules now absorb cache, `tmp/`, `build-*`, and `*.tmp` / `*.log` style paths in [../../.gitignore](../../.gitignore), but `docs/**` and `tests/**` now have explicit negate rules and are checked by [../../scripts/repo-hygiene-gate.py](../../scripts/repo-hygiene-gate.py) | Docs / Ecosystem, Test Quality | Keep the shared ignore baseline, extend explicit negate rules before adding new tracked repro roots outside `docs/**` or `tests/**`, and do not rely on review memory alone |

### 5.7 Current Closure References

Completed implementation narratives and old test-run counts remain in Git
history. Current behavior is recorded in the [IR inventory](./IM-D1-STYIOIR-CONTRACT-INVENTORY.md),
[IDE inventory](./IM-D9-IDE-LSP-SERVICE-CONTRACT-INVENTORY.md),
[feature contracts](../design/syntax/features/README.md), and
[test catalog](../../workflows/TEST-CATALOG.md). A previous closure does not
establish acceptance for a changed candidate.

## 6. Next-Stage Execution Entry

Start the next stage from checkpoint-sized implementation slices. Do not recreate the deleted long-form plan files; each slice should update this ledger, the owning runbook, the design SSOT if semantics change, and the smallest matching tests.

| Checkpoint | First output | Owner path | Required proof |
|------------|--------------|------------|----------------|
| P2 | Carry one stream/zip source combination through parser, sema, lowering, runtime, and tests | W2 / Frontend + Sema / IR + Codegen / Runtime | Affected stream-processing feature case, five-layer evidence, and parser shadow gates |
| P3 | Advance one Topology v2 compiler slice without adding legacy syntax back | W6 / Frontend + Sema / IR + Test Quality | Resource-topology unit tests, active-syntax docs update, and migration diagnostics when retired syntax is touched |
| P4 | Route resource/task pressure evidence through `styio-benchmark` while keeping Styio-side probes stable | Perf / Stability + Test Quality | `styio-benchmark` report path or documented handoff plus Styio probe gate |

## 7. Next-Stage Workstream Queue

The next stage should not be a single monolithic rewrite. Use checkpoint-sized workstreams that map to team ownership and hard gates.

| Queue | Scope | Primary teams | Depends on | Minimum gate |
|-------|-------|---------------|------------|--------------|
| W1 | Extend one accepted Sema/lowering debt family | Sema / IR, Codegen / Runtime, Test Quality | None | targeted unit coverage plus affected feature cases |
| W2 | Carry one stream/zip slice end-to-end | Frontend, Sema / IR, Codegen / Runtime, Test Quality | W1 for touched nodes | feature tests, five-layer checks, and relevant runtime/security coverage |
| W3 | Close the next IDE/LSP operational gap before expanding the public method surface | IDE / LSP, Test Quality | Stable semantic publication and runtime-drain behavior | focused IDE/LSP tests, protocol-boundary evidence, and owner docs |
| W4 | Harden the delivered `compile_plan` consumer contract for Pafio handoff | CLI / Nano, Docs / Ecosystem, Pafio coordination | None | `StyioDiagnostics.*` coverage, handoff doc update, docs audit |
| W5 | Complete nano negative-path coverage and contract hardening | CLI / Nano, Test Quality | W4 not required | nano-focused unit tests plus docs audit |
| W6 | Extend one unsupported topology selector or driver slice | Frontend, Sema / IR, Docs / Ecosystem, Test Quality | W1 strongly recommended | feature acceptance, design doc update, ADR when ownership/lifecycle semantics change |
| W7 | Keep milestone catalog and migration diagnostics aligned with active syntax | Test Quality with affected module owners | Parallel with W1-W6 | the affected feature labels from `TEST-CATALOG.md`, catalog/doc sync, no orphan fixtures |
| W8 | Continue local divergence migration from preserved branch/stash without restoring deleted governance surfaces | Docs / Ecosystem with affected implementation owners | Current upstream `nightly` governance | [LOCAL-DIVERGENCE-MIGRATION-2026-07-04.md](./LOCAL-DIVERGENCE-MIGRATION-2026-07-04.md), owning runbook updates, targeted tests |

## 8. Carry-Forward Register

The previous planning generation is removed from the current tree. The items below are registered here as future work only; this registration does not authorize or implement any item.

### 8.1 Observable Language Delivery Sequence

The observable-language roadmap has repository-owned delivery records. S1 and S2 completed their recorded acceptance; S3 has committed opt-in implementation but remains blocked on approved performance-budget evidence. A completed producer stage does not imply that every language producer or downstream integration is complete; remaining gaps live in [OBSERVABLE-DELIVERY-FOLLOW-UPS.md](./OBSERVABLE-DELIVERY-FOLLOW-UPS.md).

| Stage | Owning plan | Recorded status | Cross-repository handoff |
|---|---|---|---|
| S1 — immutable static snapshot | [PLAN-004](../plan/observable-static-snapshot/Plan.md) | Completed; full regression recorded passing | Qualified snapshot fixtures define the compiler-owned producer contract |
| S2 — delta, lineage, and bounded queries | [PLAN-005](../plan/observable-delta-query-lineage/Plan.md) | Completed; full regression recorded passing | Delta/query/lineage fixtures define the producer contract; absent Sema lineage producers remain explicit follow-ups |
| S3 — runtime and scheduler correlation | [PLAN-006](../plan/observable-runtime-correlation/Plan.md) | Blocked on approved benchmark budgets; implementation is opt-in | `styio-benchmark` owns baseline evidence and approved ceilings; default enablement requires the passing budget contract |

The broader [visual-language target](../design/Styio-Language-Design.md#mapping-text-to-a-visual-view)
requires a reviewable program hierarchy and supported module/control-flow/loop/branch/dependency facts.
The current resource-centric snapshot is not proof of that whole target: it
has no dedicated module/loop/branch kinds, scalar no-op snapshots are empty,
and anchors have file precision. Sema / IR owns fact-production proposals,
IDE / LSP reviews the consumer contract, and Codegen / Runtime owns any new
execution evidence. Before claiming broader coverage, record the accepted
contract, producer, independent consumer fixture, and completeness tests. External UI acceptance remains with Vityo.

Stage boundaries remain strict: S1 publishes static compiler-owned facts; S2 adds incremental transport and bounded lookup over those facts; S3 adds loss-accounted runtime correlation. Vityo integration, backend telemetry, cloud storage, policy, replay, and raw runtime-value capture are not folded into these producer stages.

### 8.2 Existing Carry-Forward Register

| Register ID | Carried-forward work | Current boundary / reopen condition | Owning queue |
|-------------|----------------------|-------------------------------------|--------------|
| CF-P2-M7 | P2: carry one accepted stream/zip source combination through parser, Sema, lowering, runtime, and tests | Select one independently acceptable source combination and preserve five-layer, milestone, runtime, and security evidence | W2 |
| CF-P3-TOPOLOGY | P3: advance one Topology v2 compiler slice without restoring retired syntax | Follow the canonical topology SSOT and the P0 inventory; each slice needs resource-topology tests and migration diagnostics | W6, W7 |
| CF-P4-PERF | P4: route resource/task pressure evidence through `styio-benchmark` while preserving Styio probes | External benchmark assets and an evidence handoff must exist; this repository does not absorb benchmark workload ownership | Performance / Stability, W7 |
| CF-W1 | W1: extend one accepted Sema/lowering debt family | Reopen one accepted implementation-debt family at a time with targeted unit and milestone evidence | W1 |
| CF-W2 | W2: deliver the selected stream slice end to end | Depends on W1 only for touched nodes | W2 |
| CF-W3 | W3: close one IDE/LSP operational gap before feature expansion | Preserve runtime drain, semantic identity, cache invalidation, and protocol-boundary tests | W3 |
| CF-W4 | W4: harden the compiler-side `compile_plan` consumer contract | Keep project/package lifecycle in Pafio; require machine-readable diagnostics and handoff sync | W4 |
| CF-W5 | W5: complete nano negative-path and contract-edge coverage | Add focused marker, integrity, and mutually-exclusive guard evidence without expanding package-manager scope | W5 |
| CF-W6 | W6: extend one unsupported topology selector or driver slice | W1 is strongly recommended; retired resource spellings remain negative-only | W6 |
| CF-W7 | W7: align milestones, migration diagnostics, catalogs, and active syntax | Runs with each affected implementation queue and must leave no orphan fixture | W7 |
| CF-W8 | W8: migrate preserved local divergence in checkpoint-sized slices | Do not restore removed governance or planning surfaces; require owning runbook and focused tests | W8 |
| CF-CALLABLE-Q3 | Deferred callable decisions Q3.1–Q3.3: nested contract origin, static rank-2 representation, and shallow pure subsumption | Remains `deferred/not_started` until the language owner answers the queued batch; recommendations are not authority | Sema / Type System |
| CF-CALLABLE-Q6 | Deferred callable decisions Q6.1–Q6.3: concrete instance heads, ownership/coherence, and static interface evidence | Remains `deferred/not_started` until nominal ownership prerequisites converge and the language owner answers the queued batch | Sema / Modules |
| CF-COVERAGE | Verify current required-lane source coverage | The acceptance floor remains 95% in the release-conformance inventory. A historical callable-batch measurement was below that floor; it does not establish the current revision's percentage. Run the current gate and retain its artifact/revision before claiming closure. | Test Quality, [IM-D6](./IM-D6-RELEASE-CONFORMANCE-INVENTORY.md) |
| CF-CORRECTNESS | Select the next compiler-correctness cluster after the completed loop-control closure | Compare remaining explicit feature-debt families, select exactly one smallest independent cluster, freeze semantics, and require final focused acceptance | W1 plus affected owner |

## 9. Rules for Scalable Team Execution

1. Keep checkpoints independently reviewable, consistent with [../../workflows/CHECKPOINT-WORKFLOW.md](../../workflows/CHECKPOINT-WORKFLOW.md).
2. Route every checkpoint through the owning runbook in [../teams/COORDINATION-RUNBOOK.md](../teams/COORDINATION-RUNBOOK.md); do not leave cross-team review implicit.
3. Do not let package-manager UX drift back into `styio`; only compiler-side contracts belong here.
4. Preserve the implemented stdio drain and semantic publication tests when extending the public IDE surface.
5. Do not remove parser fallback or compatibility routes without the shadow/five-layer gates that already protect the nightly-first baseline.
6. When a gap is closed, update this ledger, the owning SSOT, the relevant runbook or handoff doc, and the smallest matching tests in the same merge unit.

## 10. Immediate Stage Conclusion

1. The repository is not “unfinished everywhere”; it already has a real nightly-first baseline, a real IDE core, and a real nano bootstrap contract.
2. The deepest unfinished work is concentrated in compiler completion debt: parser subset gaps, the explicit Sema/type/lowering debt families, stream/runtime combinations, and unsupported topology capabilities.
3. Package-manager expectations must stay split cleanly: `styio` now owns the compiler-side compile-plan contract baseline and its compatibility maintenance, but not a full package-manager product surface.
4. IDE next-stage work should prioritize operational closure over feature count: retain drain and identity guarantees while extending methods.
