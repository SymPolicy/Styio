# Milestone 16: Completion Engine Upgrade

**Purpose:** M16 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md) 和 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)。

**Last updated:** 2026-04-15

**Status:** Planned frozen acceptance batch

**Depends on:** M15 (type inference queries)  
**Goal:** 把补全从“能给结果”升级到“有质量的结果”。语法位置、作用域、接收者类型、参数上下文和排序策略都要进入同一条 completion pipeline。

---

## Definitions

This milestone freezes these completion dimensions:

1. `PositionKind` and expected-category-driven candidate filtering
2. scope-aware ranking across locals, imports, builtins, keywords, snippets
3. receiver-aware member completion and call-site expected-type filtering
4. best-effort completion under syntax errors

---

## Acceptance Tests

### T16.01 — Type positions only return type-shaped candidates

**Target:** new completion test  
**Suggested name:** `StyioCompletionEngine.FiltersTypePositionCandidates`

Acceptance:

- type positions prefer or restrict to type symbols
- value-only names do not outrank valid types

### T16.02 — Locals outrank imports and builtins

**Target:** new completion ranking test  
**Suggested name:** `StyioCompletionEngine.RanksLocalsAboveImportsAndBuiltins`

Acceptance:

- local bindings sort ahead of imported and builtin candidates
- shadowed names do not leak above the visible binding

### T16.03 — Member completion uses receiver type and capabilities

**Target:** new member completion test  
**Suggested name:** `StyioCompletionEngine.FiltersMembersByReceiverType`

Acceptance:

- member completion filters by receiver type/capability
- unrelated members are suppressed or strongly demoted

### T16.04 — Call argument completion uses expected types

**Target:** new call-site completion test  
**Suggested name:** `StyioCompletionEngine.UsesExpectedTypesAtCallSites`

Acceptance:

- argument-position completion considers expected parameter types
- typed locals outrank text-only matches when semantically compatible

### T16.05 — Syntax errors still return best-effort completion

**Target:** new recovery completion test  
**Suggested name:** `StyioCompletionEngine.RecoversInBrokenSyntax`

Acceptance:

- broken syntax does not crash completion
- completion still returns contextually relevant candidates where possible

---

## Implementation Tasks

### Task 16.1 — Enrich completion context
**Role:** IDE Agent  
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/Syntax.*`, shared completion DTOs  
**Action:** Expand `CompletionContext` to carry position kind, scope, receiver type, and expected categories.  
**Verify:** Build succeeds.

### Task 16.2 — Implement ranking policy
**Role:** IDE Agent  
**Files:** `src/StyioIDE/SemDB.*`, completion helpers  
**Action:** Rank locals, imports, builtins, keywords, and snippets consistently.  
**Verify:** T16.02 passes.

### Task 16.3 — Add receiver-aware and expected-type-aware filtering
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/Service.*`  
**Action:** Use typed context to filter and score members and argument-site candidates.  
**Verify:** T16.01, T16.03, and T16.04 pass.

### Task 16.4 — Preserve best-effort recovery path
**Role:** IDE Agent  
**Files:** `src/StyioIDE/Syntax.*`, `src/StyioIDE/SemDB.*`  
**Action:** Keep completion operational under incomplete or malformed code.  
**Verify:** T16.05 passes.

### Task 16.5 — Update docs
**Role:** Doc Agent  
**Files:** `docs/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`  
**Action:** Document completion ranking and filtering policy.  
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M16 is not complete unless:

1. hot completion on a 5k-line file stays within `p95 <= 50ms`
2. member completion does not force whole-file re-inference on each request

---

## Completion Criteria

M16 is complete when:

1. T16.01-T16.05 pass
2. completion quality is scope-aware and type-aware
3. syntax-error recovery remains best-effort instead of failing closed
