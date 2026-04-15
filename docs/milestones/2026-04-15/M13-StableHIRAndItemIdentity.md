# Milestone 13: Stable HIR and Item Identity

**Purpose:** M13 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md) 和 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)。

**Last updated:** 2026-04-15

**Status:** Planned frozen acceptance batch

**Depends on:** M12 (semantic query cache)  
**Goal:** IDE 语义层必须从“可用的抽取结果”升级到“稳定的 HIR 真相”。顶层项、作用域和局部绑定要拥有稳定身份，IDE 功能不再直接依赖 token 启发式。

---

## Definitions

This milestone freezes these new semantic boundaries:

1. `ModuleId`, `ItemId`, `ScopeId`, `SymbolId`, `TypeId`
2. AST-to-HIR lowering for modules, imports, functions, params, locals, blocks, resources
3. unchanged items retain identity when an unrelated item changes

This milestone does not yet require full lexical name resolution or item-level type inference reuse. Those belong to M14 and M15.

---

## Acceptance Tests

### T13.01 — HIR contains stable top-level items

**Target:** new HIR unit test  
**Suggested name:** `StyioHirBuilder.BuildsStableTopLevelItems`

Acceptance:

- module items have durable `ItemId`s
- function/import/resource summaries are present in HIR
- HIR is built from semantic lowering, not direct token scanning

### T13.02 — Unaffected items keep identity across edits

**Target:** new HIR incremental test  
**Suggested name:** `StyioHirBuilder.RetainsUnaffectedItemIdentityAcrossEdits`

Scenario:

1. build HIR for a file with multiple top-level items
2. edit the body of one item only
3. rebuild HIR

Acceptance:

- untouched items keep the same `ItemId`
- the edited item is allowed to change identity or version metadata

### T13.03 — Block scopes and local bindings are represented explicitly

**Target:** new HIR unit test  
**Suggested name:** `StyioHirBuilder.ModelsNestedScopesAndBindings`

Acceptance:

- params and locals are associated with concrete `ScopeId`s
- nested blocks are preserved
- HIR exposes enough structure for downstream name resolution

### T13.04 — IDE consumers use HIR-backed summaries

**Target:** new IDE service regression  
**Suggested name:** `StyioIdeService.UsesHirBackedDocumentSymbolsAndDefinition`

Acceptance:

- document symbols consume HIR items
- definition lookup no longer depends on token-only heuristics where HIR data exists

---

## Implementation Tasks

### Task 13.1 — Define stable semantic IDs
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/HIR.*`, related shared IDE headers  
**Action:** Introduce canonical ID types and equality/hash behavior.  
**Verify:** Build succeeds.

### Task 13.2 — Lower AST/semantic truth into HIR
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/HIR.*`, `src/StyioIDE/CompilerBridge.*`  
**Action:** Replace token-driven summaries with explicit lowering from parser/analyzer output.  
**Verify:** T13.01 passes.

### Task 13.3 — Preserve unaffected item identity
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/HIR.*`, `src/StyioIDE/SemDB.*`  
**Action:** Define and implement identity retention rules for unchanged top-level items.  
**Verify:** T13.02 passes.

### Task 13.4 — Add scope-bearing locals and blocks
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/HIR.*`  
**Action:** Materialize block scopes, params, and locals explicitly in HIR.  
**Verify:** T13.03 passes.

### Task 13.5 — Switch IDE summaries to HIR consumers
**Role:** IDE Agent  
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/Service.*`  
**Action:** Make document-symbol and definition paths use HIR-backed data.  
**Verify:** T13.04 passes.

### Task 13.6 — Update docs
**Role:** Doc Agent  
**Files:** `docs/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`  
**Action:** Document HIR shapes and identity rules.  
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M13 is not complete unless:

1. HIR build for a hot 5k-line file stays within `p95 <= 20ms`
2. editing one function body does not force unrelated item headers to be rebuilt from scratch

---

## Completion Criteria

M13 is complete when:

1. T13.01-T13.04 pass
2. HIR is the canonical IDE semantic layer for items/scopes
3. unchanged items retain stable identity across unrelated edits
