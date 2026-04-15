# Milestone 15: Type Inference Queries

**Purpose:** M15 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md) 和 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)。

**Last updated:** 2026-04-15

**Status:** Planned frozen acceptance batch

**Depends on:** M14 (name resolution and scope graph)  
**Goal:** 把类型推断从“文件级重算”拆到 item/query 级别。函数签名、函数体和接收者类型要能独立缓存，改单个函数体时不应让整文件推断失效。

---

## Definitions

This milestone freezes:

1. per-item inference query boundaries
2. separate queries for signatures and bodies
3. receiver and expected-type results consumable by hover/completion

The invalidation key may be a subtree hash, body version, or equivalent stable item-local fingerprint, but the behavior must be tested.

---

## Acceptance Tests

### T15.01 — Signature and body inference are separate queries

**Target:** new inference cache test  
**Suggested name:** `StyioTypeInference.SeparatesSignatureAndBodyQueries`

Acceptance:

- signature queries can be reused without rerunning body inference
- body queries observe signature dependencies explicitly

### T15.02 — Editing one function body does not invalidate unrelated function inference

**Target:** new incremental inference test  
**Suggested name:** `StyioTypeInference.InvalidatesOnlyEditedFunctionBody`

Acceptance:

- editing function `A` does not rebuild cached body inference for unrelated function `B`
- item-level invalidation is observable in counters or test hooks

### T15.03 — Member receiver types are available to IDE consumers

**Target:** new IDE semantic test  
**Suggested name:** `StyioIdeService.ExposesReceiverTypesForMembers`

Acceptance:

- member access sites surface receiver type information
- hover/completion can consume that type data

### T15.04 — Call-site expected types are available

**Target:** new IDE semantic test  
**Suggested name:** `StyioIdeService.ExposesCallSiteExpectedTypes`

Acceptance:

- call arguments expose parameter/type expectations
- completion context can distinguish generic expression positions from argument positions

---

## Implementation Tasks

### Task 15.1 — Define item-level inference query keys
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/HIR.*`  
**Action:** Add query keys for signatures, bodies, and typed expression sites.  
**Verify:** Build succeeds.

### Task 15.2 — Split signature inference from body inference
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/CompilerBridge.*`, `src/StyioIDE/SemDB.*`  
**Action:** Refactor inference entry points so body work no longer dominates every typed request.  
**Verify:** T15.01 passes.

### Task 15.3 — Add item-local invalidation
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/SemDB.*`  
**Action:** Invalidate only the changed item body wherever dependency rules permit.  
**Verify:** T15.02 passes.

### Task 15.4 — Surface receiver and expected-type data
**Role:** IDE Agent  
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/Service.*`  
**Action:** Thread typed context into hover and completion.  
**Verify:** T15.03 and T15.04 pass.

### Task 15.5 — Update docs
**Role:** Doc Agent  
**Files:** `docs/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`  
**Action:** Document inference query boundaries and invalidation rules.  
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M15 is not complete unless:

1. hot hover on a typed site stays within `p95 <= 80ms`
2. editing one function body does not trigger whole-file body re-inference

---

## Completion Criteria

M15 is complete when:

1. T15.01-T15.04 pass
2. type inference is query-shaped below whole-file granularity
3. IDE consumers can access receiver and expected-type information
