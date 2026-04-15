# Milestone 19: Quality and Performance Closure

**Purpose:** M19 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md) 和 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)。

**Last updated:** 2026-04-15

**Status:** Planned frozen acceptance batch

**Depends on:** M18 (IDE runtime)  
**Goal:** 在扩功能之前先收口质量。语法漂移、模糊输入、LSP transcript 和性能回归都要进入持续校验；未过门槛时不得继续扩 IDE 功能面。

---

## Definitions

This milestone freezes:

1. syntax drift corpus between Tree-sitter and Nightly parser
2. fuzz targets for syntax, completion, and LSP sync
3. reproducible performance benchmarks and latency budgets
4. cache-hit and stale-work observability

---

## Acceptance Tests

### T19.01 — Syntax drift corpus stays within the approved envelope

**Target:** new corpus/drift suite  
**Suggested name:** `StyioSyntaxDrift.CorpusMatchesApprovedEnvelope`

Acceptance:

- token boundaries, statement starts, and block structure stay within the frozen comparison envelope
- any approved exception is explicitly documented

### T19.02 — Fuzz targets do not crash or hang

**Target:** new fuzz CI entry points  
**Suggested name:** `StyioFuzzTargets.SyntaxCompletionAndLspSyncRemainStable`

Acceptance:

- syntax, completion, and LSP sync fuzzers run without crash, hang, or sanitizer failure

### T19.03 — Performance harness enforces latency budgets

**Target:** new perf harness  
**Suggested name:** `StyioIdePerf.EnforcesFrozenLatencyBudgets`

Acceptance:

- syntax, completion, hover/definition, and background-index startup budgets are checked in a reproducible harness

### T19.04 — Existing IDE/LSP regression suite remains green

Required regression surface:

1. all accepted `M11-M18` tests
2. current IDE/LSP smoke tests
3. docs audit and generated indexes

---

## Implementation Tasks

### Task 19.1 — Freeze syntax drift corpus
**Role:** Quality Agent  
**Files:** `tests/ide/`, corpus locations as needed  
**Action:** Build and maintain a representative syntax corpus compared across both parser layers.  
**Verify:** T19.01 passes.

### Task 19.2 — Add fuzz targets
**Role:** Quality Agent  
**Files:** fuzz target directories and build glue as needed  
**Action:** Add fuzz entry points for syntax, completion, and LSP sync/document updates.  
**Verify:** T19.02 passes.

### Task 19.3 — Add performance harness and reporting
**Role:** Quality Agent  
**Files:** performance test harness locations, IDE runtime instrumentation  
**Action:** Make the core latency budgets reproducible and reportable.  
**Verify:** T19.03 passes.

### Task 19.4 — Block uncontrolled scope growth
**Role:** Quality Agent  
**Files:** CI scripts, docs, or test glue as needed  
**Action:** Ensure new IDE feature work cannot bypass frozen performance gates without a documented waiver.  
**Verify:** T19.04 passes.

### Task 19.5 — Update docs
**Role:** Doc Agent  
**Files:** `docs/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`  
**Action:** Document quality gates, corpora, fuzz targets, and benchmark commands.  
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M19 is not complete unless:

1. 5k-line hot incremental syntax parse: `p95 <= 10ms`
2. 5k-line hot completion: `p95 <= 50ms`
3. 5k-line hot hover/definition: `p95 <= 80ms`
4. current repository scale, first background index: `<= 5s`

---

## Completion Criteria

M19 is complete when:

1. T19.01-T19.04 pass
2. parser drift, fuzz stability, and latency gates are frozen and automated
3. further IDE feature expansion requires a new plan or an explicit waiver
