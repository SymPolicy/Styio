# Milestone 13: Stable HIR and Item Identity

**Purpose:** M13 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md) 和 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)。

**Last updated:** 2026-04-16

**Status:** Implemented

**Depends on:** M12 (semantic query cache)
**Goal:** IDE 语义层必须从“可用的抽取结果”升级到“稳定的 HIR 真相”。顶层项、作用域和局部绑定要拥有稳定身份，IDE 功能不再直接依赖 token 启发式。

---

## Definitions

This milestone freezes these new semantic boundaries:

1. `ModuleId`, `ItemId`, `ScopeId`, `SymbolId`, `TypeId`
2. AST-to-HIR lowering for modules, imports, functions, params, locals, blocks, resources
3. unchanged items retain identity when an unrelated item changes

This milestone does not yet require full lexical name resolution or item-level type inference reuse. Those belong to M14 and M15.

## Frozen Implementation Decisions

1. `SnapshotId` still changes on every document transition.
2. `ItemId`, `ScopeId`, and `SymbolId` are stable within one IDE session for unchanged semantic identities; cross-restart stability is not required in M13.
3. `ModuleId` maps to the file's `FileId` in v1, so one source file owns one primary HIR module.
4. Top-level `ItemId` identity is keyed by `FileId` ownership plus `ItemKind`, declared name, and same-name ordinal. Body edits and range shifts keep the same item identity; renames create a new identity.
5. `HirItem` represents module-level semantic items. `HirSymbol` represents name bindings and can point back to an item. Params and locals remain symbols associated with concrete scopes.
6. `CompilerBridge` extracts AST/analyzer-backed semantic item facts while the Nightly AST is alive. `HirBuilder` binds those facts to edit-time syntax ranges and uses token scanning only for range binding and recovery fallback.
7. Top-level `@import { ... }` declarations contribute import items through `ExtPackAST`; compatibility dot paths are normalized to canonical slash form before HIR identity assignment.
8. `SemanticDB` owns a per-file `HirIdentityStore`; M12 query caches still invalidate by snapshot, but the identity store survives same-file snapshot changes.
9. The 5k-file latency target remains a local performance goal in M13. CI gates structural identity reuse now; full p95 benchmark harness details are deferred to M19.

## Implemented Surface

M13 is implemented in [../../../src/StyioIDE/CompilerBridge.hpp](../../../src/StyioIDE/CompilerBridge.hpp), [../../../src/StyioIDE/HIR.hpp](../../../src/StyioIDE/HIR.hpp), [../../../src/StyioIDE/HIR.cpp](../../../src/StyioIDE/HIR.cpp), and [../../../src/StyioIDE/SemDB.cpp](../../../src/StyioIDE/SemDB.cpp). `SemanticSummary::items` carries semantic item facts; `HirModule::items`, `HirModule::scopes`, and `HirModule::symbols` are the IDE-facing semantic layer.

---

## Acceptance Tests

### T13.01 — HIR contains stable top-level items

**Target:** new HIR unit test
**Suggested name:** `StyioHirBuilder.BuildsStableTopLevelItems`

Acceptance:

- module items have durable `ItemId`s
- function/import/resource summaries are present in HIR
- import summaries use canonical slash names regardless of source compatibility spelling
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
**Files:** `docs/external/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`
**Action:** Document HIR shapes and identity rules.
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Targets

M13 keeps the original latency goal visible, but only structural identity reuse is a hard CI gate in this milestone. The full p95 benchmark harness and corpus are deferred to M19.

1. Target: HIR build for a hot 5k-line file stays within `p95 <= 20ms`
2. Hard gate: editing one function body does not change unrelated item identity or fingerprint

---

## Completion Criteria

M13 is complete when:

1. T13.01-T13.04 pass
2. HIR is the canonical IDE semantic layer for items/scopes
3. unchanged items retain stable identity across unrelated edits
