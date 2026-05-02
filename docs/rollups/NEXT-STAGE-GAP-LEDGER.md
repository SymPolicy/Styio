# Next-Stage Gap Ledger

**Purpose:** Provide the active, evidence-based phase summary for repository-wide unfinished work so maintainers can split the next stage into checkpoint-sized, multi-team deliveries without creating parallel truths.

**Last updated:** 2026-04-22

**Status:** Active collaboration ledger. This file distinguishes:

1. what the `styio` repository has already delivered,
2. what is still missing inside `styio`,
3. what is intentionally owned by `styio-spio` or another adjacent repository.

## 1. How to Use This Ledger

1. Start from [CURRENT-STATE.md](./CURRENT-STATE.md), then use this file to decide next-stage ownership and sequencing.
2. For checkpoint-sized execution planning, use [../plans/Next-Stage-Checkpoint-Tree.md](../plans/Next-Stage-Checkpoint-Tree.md).
3. Treat each gap below as a coordination item, not as a free-form backlog note. Every implementation checkpoint should point back to one or more ledger items.
4. When a gap changes status, update this file, the owning runbook, and the relevant SSOT or handoff document in the same checkpoint.
5. Do not use this file to redefine language semantics, package-manager product scope, or IDE public behavior. Those still belong to the owning SSOTs.

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
| Tests / Quality | Core suites exist and M8 positive smoke coverage is now in the milestone matrix, but M6 catalog alignment and negative-path package coverage still have holes | Coverage closure must run in parallel with implementation closure |

## 4. Responsibility Split: `styio` vs `styio-spio`

| Area | Owning repo | Status in `styio` | Notes |
|------|-------------|-------------------|-------|
| Nano package materialization, static local registry consume/publish, compiler capability reporting | `styio` | Delivered baseline | See [../external/for-spio/Styio-Nano-Spio-Coordination.md](../external/for-spio/Styio-Nano-Spio-Coordination.md) |
| Full package-manager UX: `install`, `use`, `search`, `vendor`, `pin`, dependency resolution, lockfiles, remote registry protocol | `styio-spio` | Out of scope here | See [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md) |
| `spio build/check/run/test` live compile-plan handoff | `styio` producer contract, `styio-spio` consumer integration | Delivered baseline in `styio` | `--machine-info=json` advertises `compile_plan:[1]`; compile-plan consumer materializes artifacts / receipt / `diag_dir` diagnostics |
| Remote registry service semantics, auth/signing/trust, channel aliasing, package listing APIs | `styio-spio` | Not owned here | `styio` should not absorb this scope |

## 5. Detailed Gap Ledger

### 5.1 Frontend / Parser

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Nightly expression parser remains a constrained subset | High | Unsupported continuation guard and explicit rejections such as dot-chain-after-call still exist in [src/StyioParser/NewParserExpr.cpp](../../src/StyioParser/NewParserExpr.cpp) | Frontend, Test Quality | Convert unsupported paths into explicit checkpoint queue; each removed rejection must ship with parity tests and shadow gate evidence |
| Topology v2 resource declaration syntax is still not the running compiler path | High | Parser comment still marks top-level `@name : [|n|] := { driver }` and `expr -> $name` as not implemented in [src/StyioParser/Parser.cpp](../../src/StyioParser/Parser.cpp); design doc still marks top-level `ResourceDecl` and shadow-write rule as not implemented in [../design/Styio-Resource-Topology.md](../design/Styio-Resource-Topology.md) | Frontend, Sema / IR, Docs / Ecosystem | Treat Topology v2 as a dedicated migration milestone, not an incidental syntax tweak |
| Handle / capability / failure-type unification is still target design rather than active compiler behavior | Medium | Design doc explicitly says the model is not fully implemented and lists current ad hoc special cases in [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md) | Frontend, Sema / IR, Codegen / Runtime | Decide whether next stage closes this model or deliberately continues with local special-case patches |

### 5.2 Sema / IR

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Placeholder lowering remains widespread | High | Representative `SGConstInt(0)` placeholders still exist for multiple AST kinds in [src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) | Sema / IR, Codegen / Runtime, Test Quality | Replace silent placeholder lowering with real lowering or explicit typed failure; do not keep placeholder nodes on active execution paths |
| Type inference coverage remains structurally incomplete | High | Empty visitors still exist for active AST families such as `CommentAST`, `InfiniteAST`, `FmtStrAST`, `ForwardAST`, and anonymous functions in [src/StyioSema/TypeInfer.cpp](../../src/StyioSema/TypeInfer.cpp) | Sema / IR, Test Quality | Build an explicit inventory of empty visitors and classify each as dead syntax, intentional no-op, or implementation debt |
| State inline clone path still has unsupported-node fallthrough | Medium | Unsupported AST fallback remains in [src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) | Sema / IR, Test Quality | Continue shrinking the unsupported clone surface until state-helper inlining is total for accepted language forms |

### 5.3 Codegen / Runtime

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| M7 multi-stream processing is not complete end-to-end | High | `IterSeqAST` exists in parser output in [src/StyioParser/Parser.cpp](../../src/StyioParser/Parser.cpp), but type inference is empty in [src/StyioSema/TypeInfer.cpp](../../src/StyioSema/TypeInfer.cpp) and IR lowering is still a placeholder in [src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) | Frontend, Sema / IR, Codegen / Runtime | Pick one accepted M7 slice and carry it through parser, sema, lowering, runtime, and milestone tests in one checkpoint chain |
| Zip lowering still supports only a narrow source set | High | Unsupported source combinations still throw in [src/StyioCodeGen/CodeGenG.cpp](../../src/StyioCodeGen/CodeGenG.cpp) | Codegen / Runtime, Sema / IR, Test Quality | Expand supported combinations according to M7 acceptance order, not ad hoc one-off cases |
| Some accepted runtime-oriented syntax still depends on special-case routing rather than a unified protocol | Medium | This is reflected both in current parser/analyzer shape and in the still-target-only capability design [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md) | Codegen / Runtime, Sema / IR | Use next-stage runtime work to reduce parser-shape-driven behavior branching |

### 5.4 CLI / Nano / `spio` Handoff

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| `compile_plan` live handoff baseline exists, but contract hardening is still narrower than a full release matrix | Medium | `--machine-info=json` now reports `compile_plan:[1]`; compile-plan success paths write artifacts / receipt / `diag_dir` diagnostics, and invalid intent / generated_by mismatch / unsupported target kind / relative-path guards / missing-outputs fallback now also have explicit machine-readable coverage in [src/main.cpp](../../src/main.cpp) and [tests/styio_test.cpp](../../tests/styio_test.cpp) | CLI / Nano, Docs / Ecosystem, `styio-spio` coordination | Keep the current v1 boundary stable while expanding compatibility-edge coverage and broader malformed-input matrices without pulling package-manager lifecycle into `styio` |
| Full package-manager lifecycle is not implemented here by design | High | CLI surface only exposes nano producer/verifier options in [src/main.cpp](../../src/main.cpp); repo boundary doc assigns package management to `styio-spio` in [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md) | CLI / Nano, Docs / Ecosystem | Preserve this boundary; do not let compiler CLI accumulate `install/use/search/vendor` responsibilities |
| Remote publish protocol is still intentionally absent | Medium | `--nano-publish` rejects HTTP(S) roots and only accepts local path or `file://` repository roots in [src/main.cpp](../../src/main.cpp) | CLI / Nano, `styio-spio` coordination | Keep publish local/static here; any remote protocol must be defined on the `styio-spio` side |
| Edge-path nano validation lacks the same depth as the happy path | Medium | Happy-path bundle/create/publish tests exist in [tests/styio_test.cpp](../../tests/styio_test.cpp), but guard/error branches remain mostly code-only in [src/main.cpp](../../src/main.cpp) | CLI / Nano, Test Quality | Add explicit negative-path tests for marker parsing, blob integrity mismatch, and mutually-exclusive CLI guards |

### 5.5 IDE / LSP

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| LSP surface is still intentionally incomplete | Medium | Current limits still list local-only, single-workspace behavior and missing `rename`, `codeAction`, and `inlayHint` in [../external/for-ide/LSP.md](../external/for-ide/LSP.md); server capabilities stop at completion/hover/definition/references/symbols/semantic tokens in [src/StyioLSP/Server.cpp](../../src/StyioLSP/Server.cpp) | IDE / LSP, Docs / Ecosystem | Expand the surface only after runtime drain and semantic identity paths remain stable under tests |
| Perf budget enforcement is split between unit and dedicated Release harnesses | Low | `StyioIdePerf.EnforcesFrozenLatencyBudgets` skips non-Release runs in [tests/ide/styio_ide_test.cpp](../../tests/ide/styio_ide_test.cpp), and M19 documents the dedicated perf harness in [../milestones/2026-04-15/M19-QualityAndPerformanceClosure.md](../milestones/2026-04-15/M19-QualityAndPerformanceClosure.md) | IDE / LSP, Perf / Stability | Preserve the dedicated Release gate, but keep the distinction visible so teams do not mistake Debug green for perf closure |

### 5.6 Tests / Quality / Perf

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Published M6 test catalog still names missing cases | Medium | Test catalog still records missing `t07`-`t10` style M6 coverage in [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) | Test Quality, Frontend, Sema / IR | Either add the missing fixtures or remove obsolete references so acceptance and catalog stay aligned |
| Package and contract negative-path testing still lags behind implementation branches | Medium | Nano create/publish guards, marker parsing, and blob verification are present in code but not closed by matching test density | Test Quality, CLI / Nano | Treat contract-edge coverage as release-blocking for any future nano handoff changes |
| Broadened ignore baselines can still hide future tracked repro fixtures outside the frozen negate roots | Low | Root ignore rules now absorb cache, `tmp/`, `build-*`, and `*.tmp` / `*.log` style paths in [../../.gitignore](../../.gitignore), but `docs/**` and `tests/**` now have explicit negate rules and are checked by [../../scripts/repo-hygiene-gate.py](../../scripts/repo-hygiene-gate.py) | Docs / Ecosystem, Test Quality | Keep the shared ignore baseline, extend explicit negate rules before adding new tracked repro roots outside `docs/**` or `tests/**`, and do not rely on review memory alone |

### 5.7 Closed Since Previous Ledger

| Closed item | Evidence | Verification |
|-------------|----------|--------------|
| `f32` dtype mapping and nearby numeric-promotion regression | `f32` maps to internal name `f32` in [src/StyioToken/Token.hpp](../../src/StyioToken/Token.hpp), and focused coverage exists in [tests/styio_test.cpp](../../tests/styio_test.cpp) (`StyioTypes.F32BuiltinMappingUsesF32InternalName`, `StyioTypes.GetMaxTypeNumericPromotionByBitWidth`) | `ctest --test-dir build-codex -R '^(StyioTypes\.F32BuiltinMappingUsesF32InternalName\|StyioTypes\.GetMaxTypeNumericPromotionByBitWidth)$' --output-on-failure` passed on 2026-04-21 |
| M8 positive smoke coverage in the automated milestone matrix | [tests/CMakeLists.txt](../../tests/CMakeLists.txt) registers M8 positive fixtures via `styio_stdout_golden_test(m8 "t*.styio" m8)`, and [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) lists `m8_t01_bounded_final_bind`, `m8_t02_bounded_read`, and `m8_t14_flex_other_var_after_final_ok` | `ctest --test-dir build-codex -R '^m8_t(01_bounded_final_bind\|02_bounded_read\|14_flex_other_var_after_final_ok)$' --output-on-failure` passed on 2026-04-21 |
| `stdio semantic drain request-loop integration` | `Server::run()` now drains runtime diagnostics on each request and `styio_ide_test` asserts the serialized loop output matches `handle + drain_runtime()` (`StyioLspServer.RunDrainsRuntimeDiagnostics`) | `ctest --test-dir build-codex --tests-regex 'StyioLspServer.RunDrainsRuntimeDiagnostics' --output-on-failure` passed on 2026-04-21 |
| `CP-B0.2 runtime scheduling freeze` | Request-loop runtime diagnostics are budgeted in `Server::run()` (`kRuntimeDrainBudgetPerLoop = 1`), `IdeService::run_idle_tasks()` drains semantic diagnostics before budgeted background work, and stale/late updates are dropped by snapshot-version sequencing in `IdeService` | `ctest --test-dir build-codex -L ide --tests-regex 'StyioLspRuntime.RuntimeDrainCanBeBudgetedForScheduling|StyioLspRuntime.IdleSliceDrainsSemanticBeforeBackgroundWork|StyioLspRuntime.RunAdvancesBackgroundWorkAsRequestDrivenFallback|StyioLspServer.RunDrainsRuntimeDiagnostics' --output-on-failure` passed on 2026-04-22 |

## 6. Next-Stage Workstream Queue

The next stage should not be a single monolithic rewrite. Use checkpoint-sized workstreams that map to team ownership and hard gates.

| Queue | Scope | Primary teams | Depends on | Minimum gate |
|-------|-------|---------------|------------|--------------|
| W1 | Inventory and retire active sema/lowering placeholders | Sema / IR, Codegen / Runtime, Test Quality | None | targeted unit coverage plus affected milestone cases |
| W2 | Carry one M7 stream/zip slice end-to-end | Frontend, Sema / IR, Codegen / Runtime, Test Quality | W1 for touched nodes | milestone tests, five-layer checks, and relevant runtime/security coverage |
| W4 | Harden the delivered `compile_plan` producer contract for `spio` handoff | CLI / Nano, Docs / Ecosystem, `styio-spio` coordination | None | `StyioDiagnostics.*` coverage, handoff doc update, docs audit |
| W5 | Complete nano negative-path coverage and contract hardening | CLI / Nano, Test Quality | W4 not required | nano-focused unit tests plus docs audit |
| W6 | Re-open Topology v2 only as a dedicated migration track | Frontend, Sema / IR, Docs / Ecosystem, Test Quality | W1 strongly recommended | milestone acceptance, design doc update, ADR when ownership/lifecycle semantics change |
| W7 | Close remaining milestone catalog gaps | Test Quality with affected module owners | Parallel with W1-W6 | `ctest -L milestone`, catalog/doc sync, no orphan fixtures |

## 7. Rules for Scalable Team Execution

1. Keep checkpoints 1-3 days wide, consistent with [../assets/workflow/CHECKPOINT-WORKFLOW.md](../assets/workflow/CHECKPOINT-WORKFLOW.md).
2. Route every checkpoint through the owning runbook in [../teams/COORDINATION-RUNBOOK.md](../teams/COORDINATION-RUNBOOK.md); do not leave cross-team review implicit.
3. Do not let package-manager UX drift back into `styio`; only compiler-side contracts belong here.
4. Do not expand the public IDE surface until the stdio runtime drain path and semantic publication discipline are closed.
5. Do not remove parser fallback or compatibility routes without the shadow/five-layer gates that already protect the nightly-first baseline.
6. When a gap is closed, update this ledger, the owning SSOT, the relevant runbook or handoff doc, and the smallest matching tests in the same merge unit.

## 8. Immediate Stage Conclusion

1. The repository is not “unfinished everywhere”; it already has a real nightly-first baseline, a real IDE core, and a real nano bootstrap contract.
2. The deepest unfinished work is concentrated in compiler completion debt: parser subset gaps, sema/type/lowering placeholders, M7 runtime closure, and Topology v2 migration debt.
3. Package-manager expectations must stay split cleanly: `styio` now owns the compiler-side compile-plan contract baseline and its compatibility maintenance, but not a full package-manager product surface.
4. IDE next-stage work should prioritize operational closure over feature count: drain semantics correctly first, then expand methods.
