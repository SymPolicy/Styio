# Next-Stage Gap Ledger

**Purpose:** Provide the active, evidence-based phase summary for repository-wide unfinished work so maintainers can split the next stage into checkpoint-sized, multi-team deliveries without creating parallel truths.

**Last updated:** 2026-08-21

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
2. M1-M10 remain the frozen language/runtime acceptance baseline; current implementation is still expected to preserve those accepted behaviors.
3. The repository is nightly-first rather than legacy-first for parser/toolchain execution.
4. `styio` already provides a bootstrap nano package producer/verifier contract, including `--nano-create`, `--nano-publish`, static repository materialization, and `--machine-info=json`.
5. The IDE stack is real and usable for completion, hover, definition, references, symbols, semantic tokens, incremental edits, and semantic query caching, but it is not feature-complete or operationally closed yet.

## 3. Executive Summary

| Stream | Current reality | Next-stage pressure |
|--------|-----------------|---------------------|
| Frontend / Parser | Nightly parser is active but still a subset with explicit unsupported continuations and fallback paths | Close subset gaps before more behavior migrates onto nightly-only assumptions |
| Sema / IR | Multiple AST families still lower to placeholders or skip type inference entirely | Highest technical debt concentration; blocks language/runtime completion |
| Codegen / Runtime | Multi-stream zip and stream-driver combinations are only partially lowered | M7 remains incomplete end-to-end |
| CLI / Nano | Bootstrap nano contract exists; full package lifecycle does not | Keep `styio` limited to compiler contracts and handoff surfaces |
| IDE / LSP | Core semantic services exist, but stdio runtime drain and several LSP methods are still absent | Close operational gaps before expanding host-facing promises |
| Tests / Quality | Core suites exist, M6 active acceptance now uses Topology v2 syntax, and resource-topology safety coverage is registered, but negative-path package and next-stage migration coverage still need expansion | Coverage closure must run in parallel with implementation closure |

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
| Topology v2 resource declaration syntax is still not the running compiler path | High | Target design now uses `@name : Type|n|`, `@name : Type|..n|`, `T..` / `T...`, `list[T]`, and `expr -> @name`; the compiler still needs the parser/type migration tracked by [../design/Styio-Resource-Topology.md](../design/Styio-Resource-Topology.md) | Frontend, Sema / IR, Docs / Ecosystem | Treat Topology v2 as a dedicated migration milestone, not an incidental syntax tweak |
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
| M7 multi-stream processing is not complete end-to-end | High | `IterSeqAST` exists in parser output in [src/StyioParser/Parser.cpp](../../src/StyioParser/Parser.cpp); Sema and lowering now reject hash-tag routing explicitly with `StyioTypeError` instead of emitting a placeholder, so the feature remains unavailable rather than silently miscompiled | Frontend, Sema / IR, Codegen / Runtime | Pick one accepted M7 slice and carry it through parser, sema, lowering, runtime, and milestone tests in one checkpoint chain |
| Zip lowering still supports only a narrow source set | High | Unsupported source combinations still throw in [src/StyioCodeGen/CodeGenG.cpp](../../src/StyioCodeGen/CodeGenG.cpp) | Codegen / Runtime, Sema / IR, Test Quality | Expand supported combinations according to M7 acceptance order, not ad hoc one-off cases |
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
| LSP surface is still intentionally incomplete | Medium | Current limits still list local-only, single-workspace behavior and missing `rename`, `codeAction`, and `inlayHint` in [../external/for-ide/LSP.md](../external/for-ide/LSP.md); server capabilities stop at completion/hover/definition/references/symbols/semantic tokens in [src/StyioServices/StyioLSP/Server.cpp](../../src/StyioServices/StyioLSP/Server.cpp) | IDE / LSP, Docs / Ecosystem | Expand the surface only after runtime drain and semantic identity paths remain stable under tests |
| Perf budget enforcement is split between unit and dedicated Release harnesses | Low | `StyioIdePerf.EnforcesFrozenLatencyBudgets` skips non-Release runs in [tests/ide/styio_ide_test.cpp](../../tests/ide/styio_ide_test.cpp), with operational guidance in [../teams/PERF-STABILITY-RUNBOOK.md](../teams/PERF-STABILITY-RUNBOOK.md) | IDE / LSP, Perf / Stability | Preserve the dedicated Release gate, but keep the distinction visible so teams do not mistake Debug green for perf closure |

### 5.6 Tests / Quality / Perf

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Next-stage migration tests need to stay tied to active syntax | Medium | [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md) now treats M6 positive coverage as Topology v2 resource syntax and keeps retired state-family spellings only as negative migration diagnostics | Test Quality, Frontend, Sema / IR | Add new positive coverage only for active syntax; keep retired spellings in negative tests with stable migration diagnostics |
| Package and contract negative-path testing still lags behind implementation branches | Medium | Nano create/publish guards, marker parsing, and blob verification are present in code but not closed by matching test density | Test Quality, CLI / Nano | Treat contract-edge coverage as release-blocking for any future nano handoff changes |
| Broadened ignore baselines can still hide future tracked repro fixtures outside the frozen negate roots | Low | Root ignore rules now absorb cache, `tmp/`, `build-*`, and `*.tmp` / `*.log` style paths in [../../.gitignore](../../.gitignore), but `docs/**` and `tests/**` now have explicit negate rules and are checked by [../../scripts/repo-hygiene-gate.py](../../scripts/repo-hygiene-gate.py) | Docs / Ecosystem, Test Quality | Keep the shared ignore baseline, extend explicit negate rules before adding new tracked repro roots outside `docs/**` or `tests/**`, and do not rely on review memory alone |

### 5.7 Closed Since Previous Ledger

| Closed item | Evidence | Verification |
|-------------|----------|--------------|
| `f32` dtype mapping and nearby numeric-promotion regression | `f32` maps to internal name `f32` in [src/StyioToken/Token.hpp](../../src/StyioToken/Token.hpp), and focused coverage exists in [tests/styio_test.cpp](../../tests/styio_test.cpp) (`StyioTypes.F32BuiltinMappingUsesF32InternalName`, `StyioTypes.GetMaxTypeNumericPromotionByBitWidth`) | `ctest --test-dir build-codex -R '^(StyioTypes\.F32BuiltinMappingUsesF32InternalName\|StyioTypes\.GetMaxTypeNumericPromotionByBitWidth)$' --output-on-failure` passed on 2026-04-21 |
| M8 positive smoke coverage in the automated milestone matrix | [tests/CMakeLists.txt](../../tests/CMakeLists.txt) registers M8 positive fixtures via `styio_stdout_golden_test(m8 "t*.styio" m8)`, and [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md) lists `m8_t01_bounded_final_bind`, `m8_t02_bounded_read`, and `m8_t14_flex_other_var_after_final_ok` | `ctest --test-dir build-codex -R '^m8_t(01_bounded_final_bind\|02_bounded_read\|14_flex_other_var_after_final_ok)$' --output-on-failure` passed on 2026-04-21 |
| `stdio semantic drain request-loop integration` | `Server::run()` now drains runtime diagnostics on each request and `styio_ide_test` asserts the serialized loop output matches `handle + drain_runtime()` (`StyioLspServer.RunDrainsRuntimeDiagnostics`) | `ctest --test-dir build-codex --tests-regex 'StyioLspServer.RunDrainsRuntimeDiagnostics' --output-on-failure` passed on 2026-04-21 |
| `CP-B0.2 runtime scheduling freeze` | Request-loop runtime diagnostics are budgeted in `Server::run()` (`kRuntimeDrainBudgetPerLoop = 1`), `IdeService::run_idle_tasks()` drains semantic diagnostics before budgeted background work, and stale/late updates are dropped by snapshot-version sequencing in `IdeService` | `ctest --test-dir build-codex -L ide --tests-regex 'StyioLspRuntime.RuntimeDrainCanBeBudgetedForScheduling|StyioLspRuntime.IdleSliceDrainsSemanticBeforeBackgroundWork|StyioLspRuntime.RunAdvancesBackgroundWorkAsRequestDrivenFallback|StyioLspServer.RunDrainsRuntimeDiagnostics' --output-on-failure` passed on 2026-04-22 |

## 6. Next-Stage Execution Entry

Start the next stage from checkpoint-sized implementation slices. Do not recreate the deleted long-form plan files; each slice should update this ledger, the owning runbook, the design SSOT if semantics change, and the smallest matching tests.

| Checkpoint | First output | Owner path | Required proof |
|------------|--------------|------------|----------------|
| P0 (closed 2026-08-01) | Inventory active Sema/lowering placeholders, clone fallthrough, generated zeros, and verifier leaves | W1 / Sema / IR | [IM-D1-STYIOIR-CONTRACT-INVENTORY.md](./IM-D1-STYIOIR-CONTRACT-INVENTORY.md) plus `python3 scripts/docs-audit.py` |
| P1 (first closure complete 2026-08-01) | Enforce IR loop-control legality without changing parser, Sema, codegen, or runtime semantics | W1 / Sema / IR + Test Quality | Focused `StyioIRContract` tests plus existing break/continue security cases |
| P2 | Carry one M7 stream/zip source combination through parser, sema, lowering, runtime, and tests | W2 / Frontend + Sema / IR + Codegen / Runtime | M7 milestone case, five-layer evidence, and parser shadow gates |
| P3 | Advance one Topology v2 compiler slice without adding legacy syntax back | W6 / Frontend + Sema / IR + Test Quality | Resource-topology unit tests, active-syntax docs update, and migration diagnostics when retired syntax is touched |
| P4 | Route resource/task pressure evidence through `styio-benchmark` while keeping Styio-side probes stable | Perf / Stability + Test Quality | `styio-benchmark` report path or documented handoff plus Styio probe gate |

## 7. Next-Stage Workstream Queue

The next stage should not be a single monolithic rewrite. Use checkpoint-sized workstreams that map to team ownership and hard gates.

| Queue | Scope | Primary teams | Depends on | Minimum gate |
|-------|-------|---------------|------------|--------------|
| W1 | Inventory and retire active sema/lowering placeholders | Sema / IR, Codegen / Runtime, Test Quality | None | targeted unit coverage plus affected milestone cases |
| W2 | Carry one M7 stream/zip slice end-to-end | Frontend, Sema / IR, Codegen / Runtime, Test Quality | W1 for touched nodes | milestone tests, five-layer checks, and relevant runtime/security coverage |
| W3 | Close the next IDE/LSP operational gap before expanding the public method surface | IDE / LSP, Test Quality | Stable semantic publication and runtime-drain behavior | focused IDE/LSP tests, protocol-boundary evidence, and owner docs |
| W4 | Harden the delivered `compile_plan` consumer contract for Pafio handoff | CLI / Nano, Docs / Ecosystem, Pafio coordination | None | `StyioDiagnostics.*` coverage, handoff doc update, docs audit |
| W5 | Complete nano negative-path coverage and contract hardening | CLI / Nano, Test Quality | W4 not required | nano-focused unit tests plus docs audit |
| W6 | Re-open Topology v2 only as a dedicated migration track | Frontend, Sema / IR, Docs / Ecosystem, Test Quality | W1 strongly recommended | milestone acceptance, design doc update, ADR when ownership/lifecycle semantics change |
| W7 | Keep milestone catalog and migration diagnostics aligned with active syntax | Test Quality with affected module owners | Parallel with W1-W6 | `ctest -L milestone`, catalog/doc sync, no orphan fixtures |
| W8 | Continue local divergence migration from preserved branch/stash without restoring deleted governance surfaces | Docs / Ecosystem with affected implementation owners | Current upstream `nightly` governance | [LOCAL-DIVERGENCE-MIGRATION-2026-07-04.md](./LOCAL-DIVERGENCE-MIGRATION-2026-07-04.md), owning runbook updates, targeted tests |

## 8. Carry-Forward Register

The previous planning generation is removed from the current tree. The items below are registered here as future work only; this registration does not authorize or implement any item.

| Register ID | Carried-forward work | Current boundary / reopen condition | Owning queue |
|-------------|----------------------|-------------------------------------|--------------|
| CF-P2-M7 | P2: carry one accepted M7 stream/zip source combination through parser, Sema, lowering, runtime, and tests | Select one independently acceptable source combination and preserve five-layer, milestone, runtime, and security evidence | W2 |
| CF-P3-TOPOLOGY | P3: advance one Topology v2 compiler slice without restoring retired syntax | Follow the canonical topology SSOT and the P0 inventory; each slice needs resource-topology tests and migration diagnostics | W6, W7 |
| CF-P4-PERF | P4: route resource/task pressure evidence through `styio-benchmark` while preserving Styio probes | External benchmark assets and an evidence handoff must exist; this repository does not absorb benchmark workload ownership | Performance / Stability, W7 |
| CF-W1 | W1: inventory and retire one active Sema/lowering placeholder family | Reopen one accepted implementation-debt family at a time with targeted unit and milestone evidence | W1 |
| CF-W2 | W2: deliver the selected M7 slice end to end | Depends on W1 only for touched nodes | W2 |
| CF-W3 | W3: close one IDE/LSP operational gap before feature expansion | Preserve runtime drain, semantic identity, cache invalidation, and protocol-boundary tests | W3 |
| CF-W4 | W4: harden the compiler-side `compile_plan` consumer contract | Keep project/package lifecycle in Pafio; require machine-readable diagnostics and handoff sync | W4 |
| CF-W5 | W5: complete nano negative-path and contract-edge coverage | Add focused marker, integrity, and mutually-exclusive guard evidence without expanding package-manager scope | W5 |
| CF-W6 | W6: continue Topology v2 only as a dedicated migration track | W1 is strongly recommended; retired resource spellings remain negative-only | W6 |
| CF-W7 | W7: align milestones, migration diagnostics, catalogs, and active syntax | Runs with each affected implementation queue and must leave no orphan fixture | W7 |
| CF-W8 | W8: migrate preserved local divergence in checkpoint-sized slices | Do not restore removed governance or planning surfaces; require owning runbook and focused tests | W8 |
| CF-CALLABLE-Q3 | Deferred callable decisions Q3.1–Q3.3: nested contract origin, static rank-2 representation, and shallow pure subsumption | Remains `deferred/not_started` until the language owner answers the queued batch; recommendations are not authority | Sema / Type System |
| CF-CALLABLE-Q6 | Deferred callable decisions Q6.1–Q6.3: concrete instance heads, ownership/coherence, and static interface evidence | Remains `deferred/not_started` until nominal ownership prerequisites converge and the language owner answers the queued batch | Sema / Modules |
| CF-CORRECTNESS | Select the next compiler-correctness cluster after the completed loop-control closure | Compare remaining explicit feature-debt families, select exactly one smallest independent cluster, freeze semantics, and require final focused acceptance | W1 plus affected owner |

## 9. Rules for Scalable Team Execution

1. Keep checkpoints 1-3 days wide, consistent with [../../workflows/CHECKPOINT-WORKFLOW.md](../../workflows/CHECKPOINT-WORKFLOW.md).
2. Route every checkpoint through the owning runbook in [../teams/COORDINATION-RUNBOOK.md](../teams/COORDINATION-RUNBOOK.md); do not leave cross-team review implicit.
3. Do not let package-manager UX drift back into `styio`; only compiler-side contracts belong here.
4. Do not expand the public IDE surface until the stdio runtime drain path and semantic publication discipline are closed.
5. Do not remove parser fallback or compatibility routes without the shadow/five-layer gates that already protect the nightly-first baseline.
6. When a gap is closed, update this ledger, the owning SSOT, the relevant runbook or handoff doc, and the smallest matching tests in the same merge unit.

## 10. Immediate Stage Conclusion

1. The repository is not “unfinished everywhere”; it already has a real nightly-first baseline, a real IDE core, and a real nano bootstrap contract.
2. The deepest unfinished work is concentrated in compiler completion debt: parser subset gaps, sema/type/lowering placeholders, M7 runtime closure, and Topology v2 migration debt.
3. Package-manager expectations must stay split cleanly: `styio` now owns the compiler-side compile-plan contract baseline and its compatibility maintenance, but not a full package-manager product surface.
4. IDE next-stage work should prioritize operational closure over feature count: drain semantics correctly first, then expand methods.
