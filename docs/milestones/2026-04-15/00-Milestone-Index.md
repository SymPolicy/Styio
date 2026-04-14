# Styio Development Milestones — Index (IDE Roadmap)

**Purpose:** 本批（`2026-04-15/`）里程碑的总索引：M11–M19 的目标、依赖链、文件路径与验收边界；它们全部派生自 [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md) 中定义的总路线图。

**Last updated:** 2026-04-15

**Date:** 2026-04-15  
**Status:** Planned frozen acceptance batch  
**Methodology:** Test-Case Driven Development. Freeze acceptance before implementation; implementation is complete only when the corresponding tests and performance gates pass.  
**Documentation policy:** [`../../specs/DOCUMENTATION-POLICY.md`](../../specs/DOCUMENTATION-POLICY.md)  
**IDE user docs:** [`../../for-ide/README.md`](../../for-ide/README.md)

---

## Overview

This batch is the frozen acceptance layer for the current IDE roadmap. It is intentionally broader than the first infrastructure slice, because the target is not just "incremental parsing works" but "Styio can approach rust-analyzer-grade semantic tooling without losing control of latency or correctness".

The milestones are grouped into four stages:

1. `M11-M12`: incremental substrate
2. `M13-M15`: stable semantic core
3. `M16-M17`: IDE feature quality and workspace scale
4. `M18-M19`: runtime hardening and quality closure

---

## Dependency Chain

```
Existing IDE Core
      │
      └── M11 ──▶ M12 ──▶ M13 ──▶ M14 ──▶ M15 ──▶ M16 ──▶ M17 ──▶ M18 ──▶ M19
```

The sequence is fixed by the plan. If the roadmap changes, update the plan first, then regenerate this batch.

---

## Milestones

| ID | Name | Goal | Acceptance Criteria |
|----|------|------|---------------------|
| **M11** | Multi-Edit Incremental Syntax | Carry true ordered edit sequences from `didChange` into VFS and Tree-sitter; stop degrading incremental clients to whole-document replacement | Multi-edit LSP transcripts and VFS/Syntax tests pass; existing IDE/LSP tests remain green |
| **M12** | Semantic Query Cache | Split SemanticDB into file and offset queries with explicit cache keys and invalidation rules | Repeated hover/completion/documentSymbol requests hit query caches within a snapshot; invalidation across snapshots works; current IDE behavior stays stable |
| **M13** | Stable HIR and Item Identity | Lower semantic truth into a stable HIR with durable item/scope identity | HIR carries stable item IDs, unchanged items retain identity, and IDE summaries consume HIR-backed data |
| **M14** | Name Resolution and Scope Graph | Replace text-oriented navigation with resolver-backed semantics | Shadowing/import rules are explicit; definition/reference/hover consume resolved symbols |
| **M15** | Type Inference Queries | Split type inference below whole-file granularity | Signature/body inference are distinct queries; unrelated function bodies stay cached across edits |
| **M16** | Completion Engine Upgrade | Make completion scope-aware, type-aware, and rank-aware | Type positions, member access, call-site expectations, and syntax-error recovery are all covered by tests |
| **M17** | Workspace Index | Scale symbol/reference lookup beyond currently open files | Open-file, background, and persistent index layers are explicit and tested |
| **M18** | IDE Runtime | Add cancellation, debounce, and background scheduling discipline | Stale work is dropped, semantic diagnostics are debounced, and foreground latency is protected |
| **M19** | Quality and Performance Closure | Freeze corpora, fuzz targets, and latency budgets | Parser drift, fuzz stability, and performance gates are automated and green |

---

## Roles

| Role | Responsibility | Tools |
|------|---------------|-------|
| **LSP Agent** | Parse ordered `contentChanges` and pass structured edits inward | `src/StyioLSP/Server.cpp`, IDE service DTOs |
| **VFS Agent** | Canonical text edit application and snapshot transitions | `src/StyioIDE/VFS.*` |
| **Syntax Agent** | Tree-sitter multi-edit reuse and syntax fallback hierarchy | `src/StyioIDE/Syntax.*`, `src/StyioIDE/TreeSitterBackend.*` |
| **Semantic Agent** | Query decomposition, cache keys, dependency graph, invalidation | `src/StyioIDE/SemDB.*`, `src/StyioIDE/CompilerBridge.*`, `src/StyioIDE/HIR.*` |
| **Test Agent** | LSP transcripts, VFS/Syntax/SemDB unit tests, regression coverage | `tests/ide/`, `tests/CMakeLists.txt` |
| **Doc Agent** | Keep `docs/plans/`, `docs/milestones/`, and `docs/for-ide/` aligned | `docs/` |

---

## Files

| Document | Content |
|----------|---------|
| `00-Milestone-Index.md` | This file (overview) |
| `M11-MultiEditIncrementalSyntax.md` | Multi-edit LSP/VFS/Tree-sitter acceptance |
| `M12-SemanticQueryCache.md` | Query-level semantic cache acceptance |
| `M13-StableHIRAndItemIdentity.md` | Stable HIR and durable semantic identity acceptance |
| `M14-NameResolutionAndScopeGraph.md` | Resolver/scope-graph acceptance |
| `M15-TypeInferenceQueries.md` | Item/query-level type inference acceptance |
| `M16-CompletionEngineUpgrade.md` | Completion quality/ranking acceptance |
| `M17-WorkspaceIndex.md` | Workspace index acceptance |
| `M18-IDERuntime.md` | Runtime scheduling/cancellation/debounce acceptance |
| `M19-QualityAndPerformanceClosure.md` | Quality, fuzz, and performance closure acceptance |
| [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md) | Active implementation sequencing |
