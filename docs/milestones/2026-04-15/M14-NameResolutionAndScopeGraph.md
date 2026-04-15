# Milestone 14: Name Resolution and Scope Graph

**Purpose:** M14 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md) 和 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)。

**Last updated:** 2026-04-15

**Status:** Planned frozen acceptance batch

**Depends on:** M13 (stable HIR and item identity)  
**Goal:** 定义、引用和补全必须建立在真实的作用域图和名字解析之上，而不是纯文本匹配。局部遮蔽、导入、内建符号和跨文件解析都要统一进同一套规则。

---

## Definitions

This milestone freezes:

1. lexical scope graph derived from HIR
2. module/import/builtin resolution rules
3. symbol-to-definition binding usable by definition/reference/hover paths

This milestone does not yet require per-item type inference reuse. That belongs to M15.

---

## Acceptance Tests

### T14.01 — Local bindings shadow imports and globals

**Target:** new resolver unit test  
**Suggested name:** `StyioNameResolver.LocalBindingsShadowImportsAndGlobals`

Acceptance:

- local variables beat imported and top-level names in the same visible scope
- shadowing behavior is deterministic and tested

### T14.02 — Imports resolve across files

**Target:** new multi-file resolver test  
**Suggested name:** `StyioNameResolver.ResolvesImportsAcrossFiles`

Acceptance:

- imported definitions bind to the correct source file/item
- missing imports stay unresolved without binding to unrelated names

### T14.03 — References use scope-aware symbol resolution

**Target:** new reference test  
**Suggested name:** `StyioIdeService.ReferencesUseScopeAwareResolution`

Acceptance:

- references for a symbol do not include unrelated same-text identifiers
- references include local and cross-file hits when legal

### T14.04 — Definition and hover use resolver output

**Target:** new IDE regression  
**Suggested name:** `StyioIdeService.DefinitionAndHoverUseResolvedSymbols`

Acceptance:

- definition returns the resolved symbol target
- hover summary is sourced from the resolved symbol, not plain token matching

---

## Implementation Tasks

### Task 14.1 — Build lexical and module scope graph
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/HIR.*`, `src/StyioIDE/SemDB.*`  
**Action:** Construct explicit scope graph nodes and parent/visibility edges.  
**Verify:** Build succeeds.

### Task 14.2 — Add builtin and import resolution
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/SemDB.*`, builtin metadata as needed  
**Action:** Resolve builtin, imported, and top-level symbols through one unified path.  
**Verify:** T14.01 and T14.02 pass.

### Task 14.3 — Rebuild definition/reference paths on resolved symbols
**Role:** IDE Agent  
**Files:** `src/StyioIDE/SemDB.*`, `src/StyioIDE/Service.*`  
**Action:** Make navigation features consume resolver output instead of text-only heuristics.  
**Verify:** T14.03 and T14.04 pass.

### Task 14.4 — Surface unresolved symbols deterministically
**Role:** Semantic Agent  
**Files:** `src/StyioIDE/SemDB.*`  
**Action:** Keep unresolved names explicit so diagnostics and IDE fallbacks remain stable.  
**Verify:** Tests cover unresolved names without false bindings.

### Task 14.5 — Update docs
**Role:** Doc Agent  
**Files:** `docs/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`  
**Action:** Document scope graph and name-resolution rules.  
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M14 is not complete unless:

1. hot definition on a 5k-line open file stays within `p95 <= 80ms`
2. reference lookup does not fall back to text scan when resolver data is available

---

## Completion Criteria

M14 is complete when:

1. T14.01-T14.04 pass
2. definition/reference/hover consume resolver output
3. lexical shadowing and import behavior are explicit and tested
