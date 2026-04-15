# Milestone 17: Workspace Index

**Purpose:** M17 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md) 和 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)。

**Last updated:** 2026-04-15

**Status:** Planned frozen acceptance batch

**Depends on:** M16 (completion engine upgrade)  
**Goal:** 把现有索引升级成真正的 workspace 级层次：打开文件热索引、后台索引和持久化索引必须分层，workspace symbol、cross-file definition、references 不能只靠当前打开文件状态。

---

## Definitions

This milestone freezes:

1. `OpenFileIndex`
2. `BackgroundIndex`
3. `PersistentIndex`
4. merge policy across these layers

Foreground IDE behavior may prefer fresh open-file state over background or persistent data, but the merge rules must be explicit.

---

## Acceptance Tests

### T17.01 — Workspace symbol searches indexed files beyond the open set

**Target:** new index integration test  
**Suggested name:** `StyioWorkspaceIndex.WorkspaceSymbolSearchIncludesBackgroundIndexedFiles`

Acceptance:

- workspace symbol returns hits from files not currently open
- stale persistent entries do not outrank fresher open-file data

### T17.02 — Cross-file definition uses indexed symbol ownership

**Target:** new definition/index test  
**Suggested name:** `StyioIdeService.DefinitionUsesWorkspaceIndexAcrossFiles`

Acceptance:

- definition resolves across files using indexed ownership data
- open-file edits still override stale index data

### T17.03 — References merge open-file and background results

**Target:** new references/index test  
**Suggested name:** `StyioIdeService.ReferencesMergeOpenFileAndBackgroundIndex`

Acceptance:

- references include both unsaved open-file hits and background-indexed hits
- duplicates are normalized

### T17.04 — Persistent index can warm a new session

**Target:** new persistence test  
**Suggested name:** `StyioWorkspaceIndex.PersistentIndexWarmsNewSession`

Acceptance:

- restarting the service can reuse persisted symbol/index data
- the warmed state remains correct after open-file overrides

---

## Implementation Tasks

### Task 17.1 — Define index schemas and merge rules
**Role:** Index Agent  
**Files:** `src/StyioIDE/Index.*`, `src/StyioIDE/SemDB.*`  
**Action:** Freeze layer responsibilities and merge precedence.  
**Verify:** Build succeeds.

### Task 17.2 — Build background indexing pipeline
**Role:** Index Agent  
**Files:** `src/StyioIDE/Index.*`, runtime helpers as needed  
**Action:** Index workspace files off the foreground path.  
**Verify:** T17.01 passes.

### Task 17.3 — Integrate indexed lookups into definition/references/workspace symbol
**Role:** IDE Agent  
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/Service.*`  
**Action:** Make navigation/search consume merged index results.  
**Verify:** T17.02 and T17.03 pass.

### Task 17.4 — Persist and reload index state
**Role:** Index Agent  
**Files:** `src/StyioIDE/Index.*`  
**Action:** Store and warm symbol/reference metadata between sessions.  
**Verify:** T17.04 passes.

### Task 17.5 — Update docs
**Role:** Doc Agent  
**Files:** `docs/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`  
**Action:** Document layer boundaries and persistence behavior.  
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M17 is not complete unless:

1. first background index of the current repository finishes within `<= 5s`
2. foreground completion/hover latency does not regress while background indexing is active

---

## Completion Criteria

M17 is complete when:

1. T17.01-T17.04 pass
2. workspace symbol/definition/references use explicit index layers
3. open-file freshness and persisted/background data have deterministic precedence
