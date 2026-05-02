# Milestone 12: Semantic Query Cache

**Purpose:** M12 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。

**Last updated:** 2026-04-15

**Status:** Implemented

**Depends on:** M11 (multi-edit incremental syntax)
**Goal:** `SemanticDB` must stop treating semantic work as one coarse `IdeSnapshot` rebuild. File-level and offset-level IDE requests should be served through explicit queries with clear cache keys and invalidation rules.

---

## Definitions

This milestone freezes the following query families:

### File-level queries

1. `syntax_tree(file, snapshot)`
2. `semantic_summary(file, snapshot)`
3. `hir_module(file, snapshot)`
4. `document_symbols(file, snapshot)`
5. `semantic_tokens(file, snapshot)`

### Offset-level queries

1. `completion_context(file, snapshot, offset)`
2. `completion(file, snapshot, offset)`
3. `hover(file, snapshot, offset)`
4. `definition(file, snapshot, offset)`
5. `references(file, snapshot, offset)`

Cache keys are frozen as:

```cpp
FileVersionKey(FileId, SnapshotId)
OffsetKey(FileId, SnapshotId, offset)
```

## Frozen Implementation Decisions

1. M12 implements same-snapshot query caching only. It does not introduce item-level dependency tracking or persistent semantic cache reuse.
2. File-level query caches are keyed by `FileVersionKey(FileId, SnapshotId)`.
3. Offset-level query caches are keyed by `OffsetKey(FileId, SnapshotId, offset)`.
4. A snapshot transition invalidates the changed file's query state. Unrelated open files are not invalidated in M12.
5. `didClose` / `SemanticDB::drop_open_file` clears open-file query state and the Tree-sitter incremental tree for that file.
6. Cache instrumentation is internal hit/miss counters exposed through `SemanticDB::query_stats()` for tests and diagnostics; it is not an LSP-facing telemetry API.

## Implemented Surface

M12 is implemented in [../../../src/StyioIDE/SemDB.hpp](../../../src/StyioIDE/SemDB.hpp) and [../../../src/StyioIDE/SemDB.cpp](../../../src/StyioIDE/SemDB.cpp). `build_snapshot(path)` remains a compatibility facade, but it now composes `IdeSnapshot` from query results instead of acting as the primary cache boundary.

---

## Acceptance Tests

### T12.01 — File query cache is reused within one snapshot

**Target:** new SemDB unit test
**Suggested name:** `StyioSemanticDb.ReusesFileQueriesWithinSnapshot`

Scenario:

1. Open a document
2. Request `document_symbols` twice
3. Request `semantic_tokens` twice

Acceptance:

- repeated requests hit file-query caches
- parser/analyzer counters do not increase on the second request

### T12.02 — Offset query cache is reused within one snapshot

**Target:** new SemDB unit test
**Suggested name:** `StyioSemanticDb.ReusesOffsetQueriesWithinSnapshot`

Scenario:

1. Open a document
2. Request `hover`, `definition`, or `completion` twice at the same offset

Acceptance:

- repeated requests hit offset-query caches
- the second request does not rebuild syntax, semantic summary, or HIR

### T12.03 — New snapshot invalidates prior file and offset queries

**Target:** new SemDB unit test
**Suggested name:** `StyioSemanticDb.InvalidatesQueriesAcrossSnapshots`

Scenario:

1. Open a document and request file/offset queries
2. Change the document to create a new snapshot
3. Repeat the same requests

Acceptance:

- old caches are not reused across snapshots
- new results reflect the updated text

### T12.04 — Closing a file drops open-file query state

**Target:** new SemDB unit test
**Suggested name:** `StyioSemanticDb.DropsOpenFileQueryStateOnClose`

Acceptance:

- closing the file drops open-file query caches
- reopening the file rebuilds required open-file state cleanly

### T12.05 — Existing IDE APIs remain behaviorally stable

Required existing tests:

1. `StyioIdeService.DocumentSymbolsHoverDefinitionAndCompletion`
2. `StyioLspServer.HandlesInitializeOpenAndCompletion`
3. M11 acceptance tests

---

## Implementation Tasks

### Task 12.1 — Define query keys and cache containers
**Role:** Semantic Agent
**Files:** `src/StyioIDE/SemDB.*`
**Action:** Introduce file-level and offset-level cache key types plus explicit cache maps per query family.
**Verify:** Build succeeds.

### Task 12.2 — Split syntax/semantic/HIR file queries
**Role:** Semantic Agent
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/CompilerBridge.*`, `src/StyioIDE/HIR.*`
**Action:** Refactor coarse snapshot recomputation into independently resolved file queries.
**Verify:** T12.01 passes.

### Task 12.3 — Split document symbols and semantic tokens into query consumers
**Role:** Semantic Agent
**Files:** `src/StyioIDE/SemDB.*`
**Action:** Make `document_symbols` and `semantic_tokens` depend on file queries instead of recomputing the whole snapshot.
**Verify:** T12.01 passes for both query kinds.

### Task 12.4 — Split hover/completion/definition/references into offset queries
**Role:** Semantic Agent
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/Service.*`
**Action:** Cache offset-based results per `(FileId, SnapshotId, offset)`.
**Verify:** T12.02 passes.

### Task 12.5 — Implement invalidation and close-file eviction
**Role:** Semantic Agent
**Files:** `src/StyioIDE/SemDB.*`
**Action:** Drop all file and offset query caches for a file when its snapshot changes or the file is closed.
**Verify:** T12.03 and T12.04 pass.

### Task 12.6 — Add instrumentation for cache hits and misses
**Role:** Semantic Agent
**Files:** `src/StyioIDE/SemDB.*`, tests if needed
**Action:** Expose counters or test hooks proving whether a request hit cache or rebuilt data.
**Verify:** T12.01–T12.04 can assert real reuse instead of only identical outputs.

### Task 12.7 — Keep `build_snapshot(path)` as migration façade
**Role:** Semantic Agent
**Files:** `src/StyioIDE/SemDB.*`
**Action:** Preserve current callers while internally composing an `IdeSnapshot` from query results.
**Verify:** T12.05 passes.

### Task 12.8 — Update docs
**Role:** Doc Agent
**Files:** `docs/external/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`
**Action:** Document query boundaries, cache keys, and invalidation semantics.
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M12 is not complete unless these gates hold:

1. Repeated `document_symbols` on the same snapshot does not rerun parser/analyzer/HIR build
2. Repeated `hover` on the same snapshot/offset does not rerun parser/analyzer/HIR build
3. Repeated `completion` on the same snapshot/offset does not rerun parser/analyzer/HIR build
4. Snapshot change invalidates only the changed file's query state, not unrelated open files

---

## Completion Criteria

M12 is complete when:

1. T12.01–T12.05 pass
2. File-level and offset-level query caches are explicit in `SemDB`
3. Invalidation rules are deterministic and tested
4. Existing IDE and LSP APIs remain source-compatible for callers above `SemDB`
