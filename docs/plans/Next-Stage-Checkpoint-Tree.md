# Next-Stage Checkpoint Tree

**Purpose:** Convert the active repository-wide gap ledger into a milestone-compatible execution tree of checkpoint-sized work items. This file is an implementation sequencing document, not an acceptance SSOT.

**Last updated:** 2026-04-22

**Primary state summary:** [../rollups/NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md)  
**Coordination runbook:** [../teams/COORDINATION-RUNBOOK.md](../teams/COORDINATION-RUNBOOK.md)  
**Checkpoint workflow:** [../assets/workflow/CHECKPOINT-WORKFLOW.md](../assets/workflow/CHECKPOINT-WORKFLOW.md)  
**Active milestone batch:** [../milestones/2026-04-15/00-Milestone-Index.md](../milestones/2026-04-15/00-Milestone-Index.md)  
**Historical acceptance provenance:** archived milestone batches under `../archive/milestones/` when exact older acceptance wording is required

---

## 1. Compatibility With Milestone Management

Yes. The checkpoint method is compatible with the repository's existing milestone method as long as the roles stay distinct:

1. **Milestones** remain the frozen acceptance layer.
2. **Checkpoint trees** remain the implementation and coordination layer.
3. **Gap ledgers** remain the phase-state summary layer.

The relationship is:

```text
Gap Ledger
   └── Workstream
         └── Checkpoint
               └── Milestone tag / acceptance target
```

### 1.1 Rules

1. A checkpoint must never replace a milestone document.
2. Every checkpoint must name one primary tag:
   - an existing milestone tag such as `M18` or `M19`,
   - a historical debt tag such as `Debt-M7` or `Debt-M8`,
   - or a bridge tag such as `Bridge-SPIO` when the work is a compiler-side contract rather than a language milestone.
3. If a checkpoint changes acceptance or semantics, the milestone or design SSOT must be updated in the same delivery.
4. If a checkpoint only sequences implementation under an already-frozen acceptance, the milestone docs stay unchanged.
5. When enough checkpoints accumulate into a new acceptance wave, freeze that wave as a new milestone batch instead of keeping it forever in plan-only form.

### 1.2 Recommended Tagging Model

| Tag class | Meaning | Example |
|-----------|---------|---------|
| `Mxx` | Active milestone implementation or closure work under an existing frozen batch | `M18`, `M19` |
| `Debt-Mxx` | Historical acceptance exists but the running implementation still has closure debt | `Debt-M7`, `Debt-M8` |
| `Bridge-*` | Cross-repo or contract work that is not itself a language acceptance milestone | `Bridge-SPIO` |
| `Infra-*` | Internal support work with no direct language milestone owner | `Infra-TestMatrix` |

This preserves milestone management instead of bypassing it.

## 2. Current Execution Axes

The current stage is not one linear track. It needs four coordinated axes:

1. **Compiler debt closure:** parser subset gaps, sema/type/lowering placeholders, M7 runtime completion, Topology v2 readiness.
2. **IDE closure:** stdio runtime drain, runtime scheduling discipline, feature-surface gating.
3. **Compiler-to-`spio` bridge:** minimal `compile_plan` contract plus nano contract hardening.
4. **Test matrix closure:** milestone registration gaps and negative-path contract coverage.

## 3. Task Tree

### 3.1 Track A — Compiler Debt Closure

#### A0. Inventory and hard bug cleanup

- `CP-A0.1` `Debt-M7` / `Debt-M8`
  - Goal: build an explicit inventory of active sema/type/lowering placeholders and classify them as dead syntax, intentional no-op, or implementation debt.
  - Owners: Sema / IR, Codegen / Runtime, Test Quality.
  - Deliverables:
    - placeholder inventory table,
    - empty-visitor inventory table,
    - explicit closure order for live execution paths.
  - Minimum gate:
    - targeted unit coverage for any changed nodes,
    - docs update in the gap ledger if the inventory changes priority.

- `CP-A0.2` `Debt-M8` — **Closed 2026-04-21**
  - Goal: fix the visible `f32 -> f64` dtype table bug before broader type-system work.
  - Owners: Frontend, Sema / IR, Test Quality.
  - Deliverables:
    - dtype mapping fix,
    - focused regression test.
  - Minimum gate:
    - closed by `StyioTypes.F32BuiltinMappingUsesF32InternalName` and `StyioTypes.GetMaxTypeNumericPromotionByBitWidth`.

#### A1. M7 stream and zip closure

- `CP-A1.1` `Debt-M7`
  - Goal: carry `IterSeqAST` through type inference and IR lowering for one accepted end-to-end path.
  - Owners: Frontend, Sema / IR, Codegen / Runtime.
  - Deliverables:
    - non-placeholder `IterSeqAST` type path,
    - non-placeholder lowering path,
    - one accepted end-to-end fixture.
  - Minimum gate:
    - relevant M7 fixtures,
    - five-layer checks for affected stages.

- `CP-A1.2` `Debt-M7`
  - Goal: expand zip lowering beyond the current narrow source matrix in a milestone-driven order.
  - Owners: Codegen / Runtime, Sema / IR, Test Quality.
  - Deliverables:
    - supported-source matrix,
    - one tranche of new lowering support,
    - negative tests for still-unsupported combinations.
  - Minimum gate:
    - affected M7 and security/runtime tests,
    - explicit diagnostics for unsupported remainder.

- `CP-A1.3` `Debt-M7`
  - Goal: reconcile implementation with the frozen M7 acceptance docs and mark what remains open.
  - Owners: Frontend, Sema / IR, Docs / Ecosystem, Test Quality.
  - Deliverables:
    - updated M7 status note,
    - aligned test catalog entries,
    - reduced mismatch between milestone text and executable coverage.
  - Minimum gate:
    - `ctest -L milestone`,
    - docs audit.

#### A2. Topology v2 and resource-model migration readiness

- `CP-A2.1` `Debt-M8`
  - Goal: freeze the actual next-step acceptance boundary for Topology v2 instead of leaving it as a broad target-design bucket.
  - Owners: Frontend, Sema / IR, Docs / Ecosystem.
  - Deliverables:
    - narrowed M8/Topology scope,
    - explicit phase boundary between current M6 canonical path and next migration slice.
  - Minimum gate:
    - design and plan doc alignment,
    - no contradiction left open between topology docs and implementation plan.

- `CP-A2.2` `Debt-M8` — **Closed 2026-04-21**
  - Goal: wire positive M8 smoke coverage into the milestone matrix once the acceptance slice is frozen.
  - Owners: Test Quality, Frontend, Sema / IR.
  - Deliverables:
    - registered positive fixtures,
    - synchronized test catalog.
  - Minimum gate:
    - closed by `styio_stdout_golden_test(m8 "t*.styio" m8)` registration plus passing `m8_t01_bounded_final_bind`, `m8_t02_bounded_read`, and `m8_t14_flex_other_var_after_final_ok`.

- `CP-A2.3` `Debt-M8`
  - Goal: implement the first real Topology v2 slice only after `CP-A2.1` freezes the scope.
  - Owners: Frontend, Sema / IR, Codegen / Runtime, Test Quality.
  - Deliverables:
    - parser + AST + semantics for the chosen slice,
    - no silent overlap with current M6 path.
  - Minimum gate:
    - milestone tests for the chosen slice,
    - docs and ADR when lifecycle/ownership rules change.

### 3.2 Track B — IDE / LSP Closure

#### B0. Runtime publication closure

- `CP-B0.1` `M18`
  - Goal: make stdio LSP automatically drain debounced semantic diagnostics instead of relying on external host polling.
  - Owners: IDE / LSP.
  - Deliverables:
    - stdio runtime drain integration,
    - regression tests for delayed semantic publication.
  - Minimum gate:
    - `ctest --test-dir build/default -L ide`,
    - `docs/external/for-ide/` update,
    - docs audit.

- `CP-B0.2` `M18` — **Closed 2026-04-22**
  - Goal: freeze idle-time scheduling behavior around runtime drain, debounce, and stale-work discard.
  - Owners: IDE / LSP, Perf / Stability.
  - Deliverables:
    - explicit scheduling contract,
    - runtime counters or tests that prove publication order and stale-work dropping.
  - Minimum gate:
    - closed by `ctest --test-dir build/default -L ide --output-on-failure`, including `StyioLspRuntime.RuntimeDrainCanBeBudgetedForScheduling`, `StyioLspRuntime.IdleSliceDrainsSemanticBeforeBackgroundWork`, and `StyioLspRuntime.RunAdvancesBackgroundWorkAsRequestDrivenFallback`.

#### B1. Surface expansion policy

- `CP-B1.1` `M18` / `M19`
  - Goal: freeze whether `rename`, `codeAction`, and `inlayHint` enter the next implementation wave or stay deferred.
  - Owners: IDE / LSP, Docs / Ecosystem.
  - Deliverables:
    - backlog decision note,
    - updated public-surface statement in IDE docs.
  - Minimum gate:
    - docs audit only if no code changes,
    - full IDE gate if server capabilities change.

- `CP-B1.2` `M19`
  - Goal: keep the distinction clear between normal Debug/unit green and Release perf closure.
  - Owners: IDE / LSP, Perf / Stability.
  - Deliverables:
    - explicit perf-gate note wherever contributors are likely to misread unit green as latency closure.
  - Minimum gate:
    - docs audit,
    - Release perf gate if harness behavior changes.

### 3.3 Track C — CLI / Nano / `spio` Bridge

#### C0. Compile-plan handoff

- `CP-C0.1` `Bridge-SPIO`
  - Goal: define the minimal `compile_plan` producer contract that `spio` can consume for build/run/test.
  - Owners: CLI / Nano, Docs / Ecosystem, `styio-spio` coordination.
  - Deliverables:
    - contract shape,
    - ownership boundary note,
    - explicit non-goals to keep package-manager lifecycle out of `styio`.
  - Minimum gate:
    - handoff doc update,
    - `StyioDiagnostics.*` or equivalent CLI contract tests,
    - docs audit.

- `CP-C0.2` `Bridge-SPIO`
  - Goal: expose the agreed compile-plan contract through `--machine-info=json` and any matching producer path.
  - Owners: CLI / Nano, Test Quality.
  - Deliverables:
    - machine-info update,
    - regression tests,
    - no ambiguity between reported and actual capability set.
  - Minimum gate:
    - targeted diagnostics tests,
    - docs audit.

#### C1. Nano contract hardening

- `CP-C1.1` `Bridge-SPIO`
  - Goal: add negative-path regression coverage for nano marker parsing, blob integrity mismatch, and CLI guard conflicts.
  - Owners: CLI / Nano, Test Quality.
  - Deliverables:
    - contract-edge tests for create/publish/consume,
    - no silent drift between code branches and test coverage.
  - Minimum gate:
    - targeted nano/diagnostics tests,
    - docs audit if contract text changes.

- `CP-C1.2` `Bridge-SPIO`
  - Goal: keep local/static publish semantics explicit and prevent remote-registry creep into the compiler CLI.
  - Owners: CLI / Nano, Docs / Ecosystem.
  - Deliverables:
    - clarified help/docs wording if needed,
    - boundary audit against `styio-spio` responsibilities.
  - Minimum gate:
    - docs audit,
    - CLI tests if flags or diagnostics change.

### 3.4 Track D — Test Matrix and Catalog Closure

#### D0. Milestone registration and catalog alignment

- `CP-D0.1` `Infra-TestMatrix`
  - Goal: reconcile missing M6 catalog entries with the actual repository test corpus.
  - Owners: Test Quality, Frontend, Sema / IR.
  - Deliverables:
    - either add the missing fixtures,
    - or remove stale catalog references.
  - Minimum gate:
    - `ctest -L milestone`,
    - docs audit.

- `CP-D0.2` `Infra-TestMatrix` — **Closed by `CP-A2.2` on 2026-04-21**
  - Goal: wire M8 positive-path coverage into normal automation once the semantics are frozen.
  - Owners: Test Quality, Frontend, Sema / IR.
  - Deliverables:
    - `tests/CMakeLists.txt` registration,
    - updated workflow catalog.
  - Minimum gate:
    - closed by the same M8 positive registration and targeted CTest evidence as `CP-A2.2`.

#### D1. Cross-stream contract coverage

- `CP-D1.1` `Bridge-SPIO`
  - Goal: make nano contract-edge coverage a release-blocking part of the bridge surface.
  - Owners: Test Quality, CLI / Nano.
  - Deliverables:
    - named negative-path suite or labels,
    - explicit regression surface for contract failures.
  - Minimum gate:
    - targeted nano contract tests.

- `CP-D1.2` `Debt-M7` / `M18`
  - Goal: keep new compiler and IDE closure work paired with the smallest matching regression tests rather than deferring coverage to the end.
  - Owners: Test Quality with module owners.
  - Deliverables:
    - per-checkpoint test additions,
    - no “implementation first, acceptance later” backlog accumulation.
  - Minimum gate:
    - module-specific tests for each touched checkpoint.

## 4. Detailed Team Division

This section is the team-facing execution view. It does not replace the runbooks; it translates the current checkpoint tree into day-to-day collaboration expectations.

### 4.1 Coordination Owner

Primary role:

1. Keep the wave plan, checkpoint queue, and cross-team dependency order explicit.
2. Refuse merges that have code but no primary checkpoint ID or no milestone/bridge tag.
3. Make sure every checkpoint writes recovery notes into `docs/history/YYYY-MM-DD.md`.
4. Route review to the correct owning runbook when a checkpoint crosses team boundaries.

What this role owns directly:

1. Wave composition.
2. Dependency arbitration.
3. Checkpoint-size discipline.
4. Cross-team blockers and handoff timing.

What this role does not own:

1. Language semantics.
2. LLVM/runtime correctness.
3. IDE public behavior.
4. Package-manager product scope.

Required closeout for every checkpoint:

1. one primary checkpoint ID,
2. one primary milestone/bridge tag,
3. one owning runbook,
4. one smallest reproducible command set,
5. one explicit next-step note if the checkpoint is not the terminal slice.

### 4.2 Frontend Team

Primary responsibility in this stage:

1. Parser subset closure.
2. Topology v2 syntax-boundary freeze and parser migration.
3. Any token or parse-route change required by M7 or M8 debt closure.

Primary checkpoints:

1. `CP-A0.2`
2. `CP-A1.1` as co-owner
3. `CP-A2.1`
4. `CP-A2.3` as co-owner

Mandatory review participation:

1. `CP-D0.1`
2. `CP-D0.2`
3. Any `Bridge-SPIO` checkpoint that changes parser-visible CLI or syntax-facing compiler behavior

Expected deliverables from Frontend:

1. exact syntax/route diff,
2. AST construction delta,
3. parser fallback or shadow-gate impact statement,
4. new or updated fixtures for accepted syntax,
5. update to syntax-facing docs when behavior changes.

Frontend should not ship alone when:

1. AST shape changes,
2. recovery output changes semantic availability,
3. parser changes alter accepted milestone behavior.

Frontend must pull in:

1. Sema / IR for AST shape or recovery-meaning changes,
2. Test Quality for new or changed fixtures,
3. Docs / Ecosystem when public syntax docs move.

### 4.3 Sema / IR Team

Primary responsibility in this stage:

1. Placeholder census and retirement.
2. Type inference coverage closure.
3. Non-placeholder lowering for active execution paths.
4. Semantic side of M7 and M8 debt closure.

Primary checkpoints:

1. `CP-A0.1`
2. `CP-A1.1` as co-owner
3. `CP-A1.2` as co-owner
4. `CP-A2.3` as co-owner

Mandatory review participation:

1. `CP-A0.2`
2. `CP-D0.1`
3. `CP-D0.2`

Expected deliverables from Sema / IR:

1. inventory of empty visitors and placeholder lowerings,
2. explicit classification of each touched node,
3. repr or IR text delta when semantics change,
4. five-layer expectations for changed nodes,
5. refusal of silent `SGConstInt(0)` style fallthrough on active paths.

Sema / IR should not ship alone when:

1. IR shape consumed by LLVM changes,
2. AST ownership or construction changes,
3. milestone or five-layer goldens need review.

Sema / IR must pull in:

1. Frontend for AST construction changes,
2. Codegen / Runtime for IR-shape changes,
3. Test Quality for pipeline and semantic-failure coverage.

### 4.4 Codegen / Runtime Team

Primary responsibility in this stage:

1. M7 execution-path closure after sema/lowering stops stubbing out.
2. Zip source-matrix expansion.
3. Runtime correctness and ownership discipline for any new stream/resource path.

Primary checkpoints:

1. `CP-A1.1` as co-owner
2. `CP-A1.2`
3. `CP-A2.3` as co-owner

Mandatory review participation:

1. `CP-A0.1`
2. `CP-C0.2` when capability output changes due to runtime truth
3. `CP-D1.2` for runtime-facing regressions

Expected deliverables from Codegen / Runtime:

1. supported-source matrix for zip/runtime behavior,
2. LLVM/runtime path for the chosen checkpoint slice,
3. explicit error category for still-unsupported combinations,
4. updated soak or benchmark note when hot paths change.

Codegen / Runtime should not ship alone when:

1. incoming IR contracts changed,
2. security or five-layer goldens changed,
3. a hot runtime loop or handle lifecycle changed materially.

Codegen / Runtime must pull in:

1. Sema / IR for every changed IR contract,
2. Test Quality for pipeline/security coverage,
3. Perf / Stability for hot-path or soak-sensitive changes.

### 4.5 IDE / LSP Team

Primary responsibility in this stage:

1. stdio semantic-drain closure,
2. runtime scheduling discipline,
3. feature-surface freeze for next IDE wave,
4. keeping M18/M19 closure honest.

Primary checkpoints:

1. `CP-B0.1`
2. `CP-B0.2`
3. `CP-B1.1`
4. `CP-B1.2`

Mandatory review participation:

1. any Frontend or Sema checkpoint that changes edit-time syntax assumptions or semantic publication shape

Expected deliverables from IDE / LSP:

1. exact before/after publication behavior,
2. LSP transcript or unit regression coverage,
3. update to `docs/external/for-ide/` when public host behavior changes,
4. explicit note when a feature stays intentionally deferred.

IDE / LSP should not ship alone when:

1. parser recovery assumptions change,
2. semantic identity/resolver behavior changes,
3. runtime scheduling changes threaten perf gates.

IDE / LSP must pull in:

1. Frontend or Sema / IR when semantic truth changes,
2. Perf / Stability when runtime work affects latency,
3. Docs / Ecosystem for public IDE surface changes.

### 4.6 CLI / Nano Team

Primary responsibility in this stage:

1. define the compiler-side `spio` handoff contract,
2. expose that contract through machine-readable surfaces,
3. harden nano create/publish/consume contract edges,
4. keep package-manager lifecycle out of the compiler.

Primary checkpoints:

1. `CP-C0.1`
2. `CP-C0.2`
3. `CP-C1.1`
4. `CP-C1.2`

Mandatory review participation:

1. any checkpoint that changes `--machine-info`,
2. any checkpoint that changes runtime capability reporting,
3. any docs change that can blur the `styio` vs `styio-spio` boundary.

Expected deliverables from CLI / Nano:

1. contract field definition,
2. precise CLI/help/diagnostic wording,
3. negative-path expectations for invalid configs or invalid repository state,
4. explicit boundary note for what still belongs to `styio-spio`.

CLI / Nano should not ship alone when:

1. runtime capability truth changes,
2. external handoff semantics change,
3. package contract docs change.

CLI / Nano must pull in:

1. Docs / Ecosystem for handoff docs,
2. Test Quality for contract-edge coverage,
3. Codegen / Runtime when capability reporting reflects execution/runtime truth.

### 4.7 Test Quality Team

Primary responsibility in this stage:

1. keep every closure slice paired with executable evidence,
2. reconcile the milestone catalog with the actual repository matrix,
3. harden nano contract-edge regression coverage,
4. prevent plan-only progress from outrunning acceptance evidence.

Primary checkpoints:

1. `CP-D0.1`
2. `CP-D0.2`
3. `CP-D1.1`
4. `CP-D1.2`

Embedded ownership across all other checkpoints:

1. Frontend syntax deltas,
2. Sema/five-layer deltas,
3. runtime/security deltas,
4. CLI/nano contract deltas,
5. IDE regression deltas.

Expected deliverables from Test Quality:

1. failing test first when accepted behavior changes,
2. correct registration in `tests/CMakeLists.txt`,
3. updated `TEST-CATALOG.md`,
4. explicit label/gate ownership for every new regression surface.

Test Quality should block a checkpoint when:

1. behavior changed but no oracle moved,
2. fixture exists but is not registered,
3. docs catalog and executable matrix disagree,
4. a negative-path contract branch still has no coverage.

### 4.8 Perf / Stability Team

Primary responsibility in this stage:

1. guard runtime and IDE closure work from regressing latency or stability,
2. own any perf-gate or soak-gate promotion,
3. provide comparable benchmark evidence for hot-path changes.

Primary checkpoints:

1. `CP-B0.2` as co-owner
2. `CP-B1.2`

Mandatory review participation:

1. `CP-A1.2` for zip/runtime hot paths,
2. any checkpoint that changes required perf or soak thresholds.

Expected deliverables from Perf / Stability:

1. baseline vs candidate route choice,
2. minimized reproduction if a regression appears,
3. explicit statement whether a checkpoint changes a required gate or only an observational route.

Perf / Stability does not accept semantics. It accepts:

1. measurement discipline,
2. comparability,
3. stability evidence,
4. threshold policy.

### 4.9 Docs / Ecosystem Team

Primary responsibility in this stage:

1. keep ledger, checkpoint tree, runbooks, indexes, archive lifecycle, and handoff docs aligned,
2. prevent repository-boundary drift,
3. keep `styio` and `styio-spio` ownership text clean and non-overlapping.

Primary checkpoints:

1. `CP-A2.1` as co-owner
2. `CP-B1.1` as co-owner
3. `CP-C0.1` as co-owner
4. `CP-C1.2` as co-owner

Mandatory review participation:

1. any checkpoint that changes public docs,
2. any checkpoint that changes milestone/plan/runbook relationships,
3. any checkpoint that moves lifecycle state in `docs/history/` or `docs/archive/`.

Expected deliverables from Docs / Ecosystem:

1. one active source of truth per topic,
2. refreshed generated indexes,
3. green docs lifecycle and audit gates,
4. explicit cross-repo boundary wording for bridge work.

Docs / Ecosystem should block a checkpoint when:

1. a new rule is copied into multiple docs instead of linked,
2. a bridge checkpoint reassigns package-manager scope into `styio`,
3. archive/history lifecycle becomes inconsistent,
4. runbook mapping is missing for changed owned files.

### 4.10 Team-to-Checkpoint Ownership Matrix

| Team | Primary checkpoints | Required support checkpoints | Must approve before merge |
|------|---------------------|------------------------------|---------------------------|
| Coordination owner | all wave planning | all cross-team checkpoints | any checkpoint lacking ID/tag/runbook/history |
| Frontend | `CP-A1.1`, `CP-A2.1`, `CP-A2.3` | `CP-D0.1` | parser route, token, AST-construction changes |
| Sema / IR | `CP-A0.1`, `CP-A1.1`, `CP-A1.2`, `CP-A2.3` | `CP-D0.1` | type, lowering, IR-shape changes |
| Codegen / Runtime | `CP-A1.2` and co-own runtime slices | `CP-A1.1`, `CP-A2.3`, `CP-C0.2` | LLVM/runtime contract changes |
| IDE / LSP | `CP-B0.1`, `CP-B0.2`, `CP-B1.1`, `CP-B1.2` | parser/sema checkpoints affecting IDE semantics | LSP surface or runtime scheduling changes |
| CLI / Nano | `CP-C0.1`, `CP-C0.2`, `CP-C1.1`, `CP-C1.2` | runtime-capability and handoff-doc checkpoints | machine-info, nano contract, bridge boundary changes |
| Test Quality | `CP-D0.1`, `CP-D1.1`, `CP-D1.2` | every implementation checkpoint | oracle, registration, or gate policy changes |
| Perf / Stability | `CP-B1.2` and co-own `CP-B0.2` | `CP-A1.2` and hot-path runtime work | perf threshold or required gate changes |
| Docs / Ecosystem | co-own `CP-A2.1`, `CP-B1.1`, `CP-C0.1`, `CP-C1.2` | every public-doc or boundary checkpoint | SSOT, lifecycle, or repo-boundary changes |

## 5. Flat Checkpoint List

| ID | Primary tag | Stream | Depends on | Primary owners | Target length | Exit gate |
|----|-------------|--------|------------|----------------|---------------|-----------|
| `CP-A0.1` | `Debt-M7` / `Debt-M8` | Sema placeholder census | none | Sema / IR, Codegen / Runtime, Test Quality | 1-2 days | targeted tests + docs sync |
| `CP-A0.2` | `Debt-M8` | `f32` bug fix | none | Frontend, Sema / IR, Test Quality | Closed 2026-04-21 | `StyioTypes.*` focused regression |
| `CP-A1.1` | `Debt-M7` | `IterSeqAST` end-to-end slice | `CP-A0.1` recommended | Frontend, Sema / IR, Codegen / Runtime | 1-3 days | M7 slice tests + five-layer |
| `CP-A1.2` | `Debt-M7` | zip source-matrix expansion | `CP-A1.1` | Codegen / Runtime, Sema / IR, Test Quality | 1-3 days | M7 + runtime/security tests |
| `CP-A1.3` | `Debt-M7` | M7 doc/test reconciliation | `CP-A1.1` and `CP-A1.2` partial | Frontend, Sema / IR, Docs / Ecosystem, Test Quality | 1 day | milestone tests + docs audit |
| `CP-A2.1` | `Debt-M8` | Topology v2 scope freeze | none | Frontend, Sema / IR, Docs / Ecosystem | 1 day | docs alignment |
| `CP-A2.2` | `Debt-M8` | M8 positive matrix | `CP-A2.1` | Test Quality, Frontend, Sema / IR | Closed 2026-04-21 | targeted M8 milestone tests |
| `CP-A2.3` | `Debt-M8` | first Topology v2 implementation slice | `CP-A2.1` | Frontend, Sema / IR, Codegen / Runtime, Test Quality | 1-3 days | milestone tests + docs/ADR |
| `CP-B0.1` | `M18` | stdio semantic drain | none | IDE / LSP | Closed 2026-04-21 | `ctest -L ide` + `StyioLspServer.RunDrainsRuntimeDiagnostics` |
| `CP-B0.2` | `M18` | runtime scheduling freeze | `CP-B0.1` | IDE / LSP, Perf / Stability | Closed 2026-04-22 | `ctest -L ide`, `StyioLspRuntime.RuntimeDrainCanBeBudgetedForScheduling`, `StyioLspRuntime.IdleSliceDrainsSemanticBeforeBackgroundWork`, `StyioLspRuntime.RunAdvancesBackgroundWorkAsRequestDrivenFallback` |
| `CP-B1.1` | `M18` / `M19` | feature-surface freeze | none | IDE / LSP, Docs / Ecosystem | <1 day | docs audit |
| `CP-B1.2` | `M19` | perf gate clarity | none | IDE / LSP, Perf / Stability | <1 day | docs audit or Release perf |
| `CP-C0.1` | `Bridge-SPIO` | compile-plan contract definition | none | CLI / Nano, Docs / Ecosystem, `styio-spio` coordination | 1-2 days | docs + diagnostics tests |
| `CP-C0.2` | `Bridge-SPIO` | machine-info/producer exposure | `CP-C0.1` | CLI / Nano, Test Quality | 1-2 days | CLI regression |
| `CP-C1.1` | `Bridge-SPIO` | nano negative-path suite | none | CLI / Nano, Test Quality | 1-2 days | targeted nano tests |
| `CP-C1.2` | `Bridge-SPIO` | local/static publish boundary hardening | `CP-C0.1` recommended | CLI / Nano, Docs / Ecosystem | <1 day | docs/CLI checks |
| `CP-D0.1` | `Infra-TestMatrix` | M6 catalog reconciliation | none | Test Quality, Frontend, Sema / IR | <1 day | milestone tests + docs |
| `CP-D0.2` | `Infra-TestMatrix` | M8 positive registration | `CP-A2.1` | Test Quality, Frontend, Sema / IR | Closed by `CP-A2.2` | targeted M8 milestone tests |
| `CP-D1.1` | `Bridge-SPIO` | contract-edge release gate | `CP-C1.1` | Test Quality, CLI / Nano | <1 day | targeted nano tests |
| `CP-D1.2` | `Debt-M7` / `M18` | no uncovered closure work | ongoing | Test Quality + module owners | ongoing | per-checkpoint tests |

## 6. Suggested Execution Waves

### Wave 1: control and truth alignment

1. `CP-A0.1` placeholder census
2. `CP-B0.1` stdio semantic drain (closed 2026-04-21)
3. `CP-C0.1` compile-plan contract definition
4. `CP-D0.1` M6 catalog reconciliation

### Wave 2: first real closure slices

1. `CP-A1.1` `IterSeqAST` end-to-end
2. `CP-B0.2` IDE runtime scheduling freeze (closed 2026-04-22)
3. `CP-C0.2` machine-info compile-plan exposure
4. `CP-C1.1` nano negative-path suite

### Wave 3: acceptance reconciliation

1. `CP-A1.2` zip matrix expansion
2. `CP-A1.3` M7 doc/test reconciliation
3. `CP-A2.1` Topology v2 scope freeze
4. `CP-D1.1` contract-edge release gate

### Wave 4: next syntax/runtime migration wave

1. `CP-A2.3` first Topology v2 implementation slice
2. `CP-B1.1` IDE feature-surface freeze
3. `CP-B1.2` perf-gate clarity closeout

## 7. Delivery Rules

1. Each checkpoint must ship code, tests, docs, and recovery notes together.
2. Every checkpoint must update [../history/](../history/) with status, next step, repro command, and risk.
3. Each checkpoint must point back to:
   - one or more gap-ledger items,
   - one primary milestone or bridge tag,
   - one owning runbook.
4. No checkpoint may silently widen repository ownership. In particular, package-manager lifecycle work must stay outside `styio`.
5. If a checkpoint changes milestone acceptance, freeze that change in milestone docs first or in the same merge.

## 8. Immediate Recommendation

Use milestone management as the reporting and acceptance spine, and use this checkpoint tree as the execution skeleton underneath it.

In practice:

1. report progress upward by milestone tag,
2. schedule work downward by checkpoint ID,
3. maintain reality in the gap ledger,
4. freeze new milestone batches only when the checkpoint wave has converged into stable acceptance.
