# Milestone 18: IDE Runtime

**Purpose:** M18 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md) 和 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)。

**Last updated:** 2026-04-15

**Status:** Planned frozen acceptance batch

**Depends on:** M17 (workspace index)  
**Goal:** IDE 守护进程必须具备真实运行时能力：请求取消、诊断 debounce、前后台任务优先级和版本守卫都要明确，否则再好的语义层也会被运行时噪音拖垮。

---

## Definitions

This milestone freezes:

1. foreground request cancellation/version guards
2. semantic diagnostic debounce
3. background task priorities and scheduling classes
4. runtime counters for latency and stale-work drops

---

## Acceptance Tests

### T18.01 — Stale completion results are dropped after a newer snapshot arrives

**Target:** new LSP/runtime transcript  
**Suggested name:** `StyioLspRuntime.DropsStaleCompletionResponses`

Acceptance:

- an older request does not overwrite or outlive a newer snapshot's result
- stale work is counted or observable

### T18.02 — Semantic diagnostics are debounced

**Target:** new runtime test  
**Suggested name:** `StyioLspRuntime.DebouncesSemanticDiagnostics`

Acceptance:

- burst edits do not trigger full semantic diagnostics for every intermediate version
- final diagnostics reflect the last visible snapshot

### T18.03 — Background indexing yields to foreground requests

**Target:** new runtime/index test  
**Suggested name:** `StyioLspRuntime.BackgroundIndexYieldsToForegroundRequests`

Acceptance:

- completion/hover/definition stay within their latency budget while background indexing is active
- background work can be paused or deprioritized

### T18.04 — Cancellation propagates through query consumers

**Target:** new runtime semantic test  
**Suggested name:** `StyioLspRuntime.CancellationPropagatesThroughSemanticQueries`

Acceptance:

- canceled work does not finish and publish stale diagnostics or responses
- query runners observe cancellation or version guard state

---

## Implementation Tasks

### Task 18.1 — Add request/version guards
**Role:** Runtime Agent  
**Files:** `src/StyioLSP/Server.cpp`, `src/StyioIDE/Service.*`  
**Action:** Tie foreground requests to snapshot/version guards so stale results can be dropped.  
**Verify:** Build succeeds.

### Task 18.2 — Add semantic diagnostics debounce
**Role:** Runtime Agent  
**Files:** `src/StyioIDE/Service.*`, runtime helpers as needed  
**Action:** Separate immediate syntax diagnostics from delayed semantic diagnostics.  
**Verify:** T18.02 passes.

### Task 18.3 — Add task classes and scheduling priorities
**Role:** Runtime Agent  
**Files:** `src/StyioIDE/Service.*`, index/runtime helpers  
**Action:** Prioritize visible-document work over indexing and maintenance work.  
**Verify:** T18.03 passes.

### Task 18.4 — Propagate cancellation
**Role:** Runtime Agent  
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/Index.*`, `src/StyioIDE/Service.*`  
**Action:** Let long-running query/index tasks observe cancellation or snapshot obsolescence.  
**Verify:** T18.01 and T18.04 pass.

### Task 18.5 — Update docs
**Role:** Doc Agent  
**Files:** `docs/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`  
**Action:** Document runtime scheduling, debounce, and cancellation semantics.  
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M18 is not complete unless:

1. hot completion remains within `p95 <= 50ms` under background load
2. hot hover/definition remain within `p95 <= 80ms` under background load

---

## Completion Criteria

M18 is complete when:

1. T18.01-T18.04 pass
2. stale work is dropped instead of surfacing to the user
3. foreground latency is protected from background maintenance
