# Next-Stage Gap Ledger

**Purpose:** Provide the active, evidence-based phase summary for repository-wide unfinished work so maintainers can split the next stage into checkpoint-sized, multi-team deliveries without creating parallel truths.

**Last updated:** 2026-05-12

**Status:** Active collaboration ledger. This file distinguishes:

1. what the `styio` repository has already delivered,
2. what is still missing inside `styio`,
3. what is intentionally owned by `styio-spio` or another adjacent repository.

## 1. How to Use This Ledger

1. Start from [CURRENT-STATE.md](./CURRENT-STATE.md), then use this file to decide next-stage ownership and sequencing.
2. Treat each gap below as a coordination item, not as a free-form backlog note. Every implementation checkpoint should point back to one or more ledger items.
3. When a gap changes status, update this file, the owning runbook, and the relevant SSOT or handoff document in the same checkpoint.
4. Do not use this file to redefine language semantics, package-manager product scope, or IDE public behavior. Those still belong to the owning SSOTs.

## 2. Current Baseline That Is Real

1. Project priorities and decision order remain fixed by [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).
2. language feature acceptance suites remain the frozen language/runtime acceptance baseline; current implementation is still expected to preserve those accepted behaviors.
3. The repository is nightly-first rather than legacy-first for parser/toolchain execution.
4. `styio` already provides a bootstrap nano package producer/verifier contract, including `--nano-create`, `--nano-publish`, static repository materialization, and `--machine-info=json`.
5. The IDE stack is real and usable for completion, hover, definition, references, symbols, semantic tokens, incremental edits, and semantic query caching, but it is not feature-complete or operationally closed yet.

## 3. Executive Summary

| Stream | Current reality | Next-stage pressure |
|--------|-----------------|---------------------|
| Frontend / Parser | Nightly parser is active but still a subset with explicit unsupported continuations and fallback paths | Close subset gaps before more behavior migrates onto nightly-only assumptions |
| Sema / IR | Multiple AST families still lower to placeholders or skip type inference entirely, though `FmtStrAST`, hash-tag iterator sequences, and internal operator fallback clusters now fail closed | Highest technical debt concentration; blocks language/runtime completion |
| Codegen / Runtime | Multi-stream zip and stream-driver combinations are only partially lowered | stream-processing remains incomplete end-to-end |
| CLI / Nano | Bootstrap nano contract exists; full package lifecycle does not | Keep `styio` limited to compiler contracts and handoff surfaces |
| IDE / LSP | Core semantic services exist, but stdio runtime drain and several LSP methods are still absent | Close operational gaps before expanding host-facing promises |
| Tests / Quality | Core suites exist, state-resource active acceptance now uses Topology v2 syntax, and resource-topology safety coverage is registered, but negative-path package and next-stage migration coverage still need expansion | Coverage closure must run in parallel with implementation closure |

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
| Topology v2 resource declaration syntax is still not the running compiler path | High | Target design now uses `@name : Type|n|`, `@name : Type|..n|`, `T..` / `T...`, `list[T]`, and `expr -> @name`; the compiler still needs the parser/type migration tracked by [../design/Styio-Resource-Topology.md](../design/Styio-Resource-Topology.md) | Frontend, Sema / IR, Docs / Ecosystem | Treat Topology v2 as a dedicated migration checkpoint, not an incidental syntax tweak |
| Handle / capability / failure-type unification is still target design rather than active compiler behavior | Medium | Design doc explicitly says the model is not fully implemented and keeps remaining capability/protocol special cases in [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md); typed stdin ingestion is no longer evidence of an active AST split | Frontend, Sema / IR, Codegen / Runtime | Decide whether next stage closes this model or deliberately continues with local special-case patches |

### 5.2 Sema / IR

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Placeholder lowering remains widespread | High | Representative `SGConstInt(0)` placeholders still exist for multiple AST kinds in [src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp), but comparison/list/logical operator fallbacks now reject unknown internal values | Sema / IR, Codegen / Runtime, Test Quality | Replace silent placeholder lowering with real lowering or explicit typed failure; do not keep placeholder nodes on active execution paths |
| Type inference coverage remains structurally incomplete | High | Empty visitors still exist for active AST families such as `CommentAST`, `InfiniteAST`, `ForwardAST`, and anonymous functions in [src/StyioSema/TypeInfer.cpp](../../src/StyioSema/TypeInfer.cpp); `FmtStrAST` now infers embedded expressions and returns `string` | Sema / IR, Test Quality | Build an explicit inventory of empty visitors and classify each as dead syntax, intentional no-op, or implementation debt |
| State inline clone path still has unsupported-node fallthrough | Medium | Unsupported AST fallback remains in [src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) | Sema / IR, Test Quality | Continue shrinking the unsupported clone surface until state-helper inlining is total for accepted language forms |

### 5.3 Codegen / Runtime

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| stream-processing processing is not complete end-to-end | High | `IterSeqAST` exists in parser output in [src/StyioParser/Parser.cpp](../../src/StyioParser/Parser.cpp), but hash-tag route semantics are not defined in active syntax; it now fails closed with a typed diagnostic instead of silently lowering to `SGConstInt(0)` | Frontend, Sema / IR, Codegen / Runtime | Decide whether to retire hash-tag routes or define them in the design SSOT; pick one accepted stream-processing slice and carry it through parser, sema, lowering, runtime, and feature tests |
| Zip lowering still supports only a narrow source set | High | Unsupported source combinations still throw in [src/StyioCodeGen/CodeGenG.cpp](../../src/StyioCodeGen/CodeGenG.cpp) | Codegen / Runtime, Sema / IR, Test Quality | Expand supported combinations according to stream-processing acceptance order, not ad hoc one-off cases |
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
| Perf budget enforcement is split between unit and dedicated Release harnesses | Low | `StyioIdePerf.EnforcesFrozenLatencyBudgets` skips non-Release runs in [tests/ide/styio_ide_test.cpp](../../tests/ide/styio_ide_test.cpp), with operational guidance in [../teams/PERF-STABILITY-RUNBOOK.md](../teams/PERF-STABILITY-RUNBOOK.md) | IDE / LSP, Perf / Stability | Preserve the dedicated Release gate, but keep the distinction visible so teams do not mistake Debug green for perf closure |

### 5.6 Tests / Quality / Perf

| Gap | Severity | Current evidence | Owning teams | Next checkpoint intent |
|-----|----------|------------------|--------------|------------------------|
| Next-stage migration tests need to stay tied to active syntax | Medium | [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) now treats state-resource positive coverage as Topology v2 resource syntax and keeps retired state-family spellings only as negative migration diagnostics | Test Quality, Frontend, Sema / IR | Add new positive coverage only for active syntax; keep retired spellings in negative tests with stable migration diagnostics |
| Package and contract negative-path testing still lags behind implementation branches | Medium | Nano create/publish guards, marker parsing, and blob verification are present in code but not closed by matching test density | Test Quality, CLI / Nano | Treat contract-edge coverage as release-blocking for any future nano handoff changes |
| Broadened ignore baselines can still hide future tracked repro fixtures outside the frozen negate roots | Low | Root ignore rules now absorb cache, `tmp/`, `build-*`, and `*.tmp` / `*.log` style paths in [../../.gitignore](../../.gitignore), but `docs/**` and `tests/**` now have explicit negate rules and are checked by [../../scripts/repo-hygiene-gate.py](../../scripts/repo-hygiene-gate.py) | Docs / Ecosystem, Test Quality | Keep the shared ignore baseline, extend explicit negate rules before adding new tracked repro roots outside `docs/**` or `tests/**`, and do not rely on review memory alone |

### 5.7 Closed Since Previous Ledger

| Closed item | Evidence | Verification |
|-------------|----------|--------------|
| `f32` dtype mapping and nearby numeric-promotion regression | `f32` maps to internal name `f32` in [src/StyioToken/Token.hpp](../../src/StyioToken/Token.hpp), and focused coverage exists in [tests/styio_test.cpp](../../tests/styio_test.cpp) (`StyioTypes.F32BuiltinMappingUsesF32InternalName`, `StyioTypes.GetMaxTypeNumericPromotionByBitWidth`) | `ctest --test-dir build-codex -R '^(StyioTypes\.F32BuiltinMappingUsesF32InternalName\|StyioTypes\.GetMaxTypeNumericPromotionByBitWidth)$' --output-on-failure` passed on 2026-04-21 |
| final-binding positive smoke coverage in the automated feature matrix | [tests/CMakeLists.txt](../../tests/CMakeLists.txt) registers final-binding positive fixtures via `styio_feature_stdout_golden_test(final_bindings "t*.styio" final_bindings)`, and [../assets/workflow/TEST-CATALOG.md](../assets/workflow/TEST-CATALOG.md) lists `final_bindings_t01_bounded_final_bind`, `final_bindings_t02_bounded_read`, and `final_bindings_t14_flex_other_var_after_final_ok` | `ctest --test-dir build/default -R '^(final_bindings_t01_bounded_final_bind|final_bindings_t02_bounded_read|final_bindings_t14_flex_other_var_after_final_ok)$' --output-on-failure` |
| `stdio semantic drain request-loop integration` | `Server::run()` now drains runtime diagnostics on each request and `styio_ide_test` asserts the serialized loop output matches `handle + drain_runtime()` (`StyioLspServer.RunDrainsRuntimeDiagnostics`) | `ctest --test-dir build-codex --tests-regex 'StyioLspServer.RunDrainsRuntimeDiagnostics' --output-on-failure` passed on 2026-04-21 |
| `CP-B0.2 runtime scheduling freeze` | Request-loop runtime diagnostics are budgeted in `Server::run()` (`kRuntimeDrainBudgetPerLoop = 1`), `IdeService::run_idle_tasks()` drains semantic diagnostics before budgeted background work, and stale/late updates are dropped by snapshot-version sequencing in `IdeService` | `ctest --test-dir build-codex -L ide --tests-regex 'StyioLspRuntime.RuntimeDrainCanBeBudgetedForScheduling|StyioLspRuntime.IdleSliceDrainsSemanticBeforeBackgroundWork|StyioLspRuntime.RunAdvancesBackgroundWorkAsRequestDrivenFallback|StyioLspServer.RunDrainsRuntimeDiagnostics' --output-on-failure` passed on 2026-04-22 |
| Format strings (`$"..."`) parser/sema/lowering closure | [src/StyioParser/Parser.cpp](../../src/StyioParser/Parser.cpp) parses token-level format strings for both parser engines, [src/StyioSema/TypeInfer.cpp](../../src/StyioSema/TypeInfer.cpp) infers embedded expressions as `string`, [src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) lowers through existing string concatenation, and [tests/features/stdio_output/t06_stdout_fmtstr.styio](../../tests/features/stdio_output/t06_stdout_fmtstr.styio) now exercises real `$"..."` syntax | `ctest --test-dir build/default -R '^(StyioParserEngine\.LegacyAndNightlyMatchOnStdioOutputFmtStringSample|stdio_output_t06_stdout_fmtstr)$' --output-on-failure` passed on 2026-05-10 |
| Hash-tag iterator sequence fail-closed | `IterSeqAST` no longer silently executes as `SGConstInt(0)`; sema/lowering paths reject undefined hash-tag routing with a `TypeError`, and [tests/styio_test.cpp](../../tests/styio_test.cpp) covers the regression | `ctest --test-dir build/default -R '^StyioDiagnostics\.IteratorSequenceHashTagRoutingFailsClosed$' --output-on-failure` passed on 2026-05-10 |
| Fuzz session lifetime and parser recovery leaks | Tokenizer accumulation, legacy main-block parsing, hash-function parts, and return-type parsing now retain RAII ownership until handoff, fuzz leak seeds are backflowed under [tests/fuzz/corpus/](../../tests/fuzz/corpus/), and [tests/security/styio_security_test.cpp](../../tests/security/styio_security_test.cpp) covers the lexer/parser paths under `CompilationSession` | `ASAN_OPTIONS=detect_leaks=1:detect_container_overflow=0 ctest --test-dir build/asan-ubsan -R '^StyioSecurity(Lexer\.UnterminatedStringAfterOwnedTokensStaysExceptionSafe\|ParserContext\.HashFunctionFuzzSeedStaysExceptionSafe)$' --output-on-failure` passed on 2026-05-12 |
| Internal operator fallback fail-closed | Unknown comparison, list, logical, and binary IR operator values now throw `StyioTypeError` instead of lowering/emitting equality, constant zero, or left-operand fallback in [src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) and [src/StyioCodeGen/CodeGenG.cpp](../../src/StyioCodeGen/CodeGenG.cpp) | `ctest --test-dir build/default -R '^StyioSecurityNightlyCodegen\.(LogicalNotAndXorLowerWithoutLeftOperandFallback\|UnsupportedInternalBinaryOperatorFailsClosed\|UnsupportedInternalLoweringOperatorsFailClosed)$' --output-on-failure` passed on 2026-05-12 |

## 6. Next-Stage Execution Entry

Start the next stage from checkpoint-sized implementation slices. Do not recreate the deleted long-form plan files; each slice should update this ledger, the owning runbook, the design SSOT if semantics change, and the smallest matching tests.

| Checkpoint | First output | Owner path | Required proof |
|------------|--------------|------------|----------------|
| P0 | Inventory active Sema/lowering placeholders by AST family and classify each as dead syntax, intentional no-op, or implementation debt | W1 / Sema / IR | Focused inventory diff plus `python3 scripts/docs-audit.py` |
| P1 | Retire one placeholder cluster by implementing real lowering or fail-closed diagnostics | W1 / Sema / IR + Codegen / Runtime | Targeted unit tests, affected `styio_pipeline` cases, and `security` when diagnostics or ownership change |
| P2 | Carry one stream-processing source combination through parser, sema, lowering, runtime, and tests | W2 / Frontend + Sema / IR + Codegen / Runtime | stream-processing feature case, five-layer evidence, and parser shadow gates |
| P3 | Advance one Topology v2 compiler slice without adding legacy syntax back | W6 / Frontend + Sema / IR + Test Quality | Resource-topology unit tests, active-syntax docs update, and migration diagnostics when retired syntax is touched |
| P4 | Route resource/task pressure evidence through `styio-benchmark` while keeping Styio-side probes stable | Perf / Stability + Test Quality | `styio-benchmark` report path or documented handoff plus Styio probe gate |

## 7. Next-Stage Workstream Queue

The next stage should not be a single monolithic rewrite. Use checkpoint-sized workstreams that map to team ownership and hard gates.

| Queue | Scope | Primary teams | Depends on | Minimum gate |
|-------|-------|---------------|------------|--------------|
| W1 | Inventory and retire active sema/lowering placeholders | Sema / IR, Codegen / Runtime, Test Quality | None | targeted unit coverage plus affected feature cases |
| W2 | Carry one stream-processing slice end-to-end | Frontend, Sema / IR, Codegen / Runtime, Test Quality | W1 for touched nodes | feature tests, five-layer checks, and relevant runtime/security coverage |
| W4 | Harden the delivered `compile_plan` producer contract for `spio` handoff | CLI / Nano, Docs / Ecosystem, `styio-spio` coordination | None | `StyioDiagnostics.*` coverage, handoff doc update, docs audit |
| W5 | Complete nano negative-path coverage and contract hardening | CLI / Nano, Test Quality | W4 not required | nano-focused unit tests plus docs audit |
| W6 | Re-open Topology v2 only as a dedicated migration track | Frontend, Sema / IR, Docs / Ecosystem, Test Quality | W1 strongly recommended | feature acceptance, design doc update, ADR when ownership/lifecycle semantics change |
| W7 | Keep feature catalog and migration diagnostics aligned with active syntax | Test Quality with affected module owners | Parallel with W1-W6 | `ctest -L language_feature`, catalog/doc sync, no orphan fixtures |

## 8. Rules for Scalable Team Execution

1. Keep checkpoints scoped to one language feature slice, one functional closure, or one minimal test-evidence group, consistent with [../assets/workflow/CHECKPOINT-WORKFLOW.md](../assets/workflow/CHECKPOINT-WORKFLOW.md).
2. Route every checkpoint through the owning runbook in [../teams/COORDINATION-RUNBOOK.md](../teams/COORDINATION-RUNBOOK.md); do not leave cross-team review implicit.
3. Do not let package-manager UX drift back into `styio`; only compiler-side contracts belong here.
4. Do not expand the public IDE surface until the stdio runtime drain path and semantic publication discipline are closed.
5. Do not remove parser fallback or compatibility routes without the shadow/five-layer gates that already protect the nightly-first baseline.
6. When a gap is closed, update this ledger, the owning SSOT, the relevant runbook or handoff doc, and the smallest matching tests in the same merge unit.

## 9. Immediate Stage Conclusion

1. The repository is not “unfinished everywhere”; it already has a real nightly-first baseline, a real IDE core, and a real nano bootstrap contract.
2. The deepest unfinished work is concentrated in compiler completion debt: parser subset gaps, sema/type/lowering placeholders, stream-processing runtime closure, and Topology v2 migration debt.
3. Package-manager expectations must stay split cleanly: `styio` now owns the compiler-side compile-plan contract baseline and its compatibility maintenance, but not a full package-manager product surface.
4. IDE next-stage work should prioritize operational closure over feature count: drain semantics correctly first, then expand methods.
