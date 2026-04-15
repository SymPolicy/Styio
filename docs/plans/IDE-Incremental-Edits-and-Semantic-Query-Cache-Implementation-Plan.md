# IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan

**Purpose:** Define the full roadmap for evolving the Styio IDE subsystem from its current MVP shape toward mature IDE-grade completion and semantic tooling. This roadmap is explicitly benchmarked against at least three established language toolchains rather than a single Rust reference point. This file path remains stable, but as of 2026-04-15 its scope has expanded from the original M11/M12 infrastructure slice into the full M11-M19 plan. Frozen acceptance targets live in [`../milestones/2026-04-15/`](../milestones/2026-04-15/), and IDE integration usage lives in [`../for-ide/README.md`](../for-ide/README.md).

**Last updated:** 2026-04-15

**Date:** 2026-04-15  
**Status:** Active implementation plan. Frozen acceptance documents live in [`../milestones/2026-04-15/`](../milestones/2026-04-15/).  
**Depends on:** Existing IDE/LSP foundation, Tree-sitter backend integration, Nightly `ParseMode::Recovery`, and current `styio_ide_core`/`styio_lspd` build targets.  
**Related docs:** [`../for-ide/BUILD.md`](../for-ide/BUILD.md), [`../for-ide/TREE-SITTER.md`](../for-ide/TREE-SITTER.md), [`../for-ide/CXX-API.md`](../for-ide/CXX-API.md), [`../specs/DOCUMENTATION-POLICY.md`](../specs/DOCUMENTATION-POLICY.md)

---

## 1. Goal

The immediate goal is not just to make the IDE core incremental. The actual target is to close the largest structural gaps between the current Styio IDE subsystem and mature language IDE toolchains:

1. true multi-edit incremental syntax updates
2. explicit query-shaped semantic caches
3. stable HIR with durable item identity
4. lexical-plus-semantic name resolution through a real scope graph
5. item-level type inference reuse
6. completion driven by syntax context, scope, receiver type, and ranking policy
7. workspace-scale symbol/reference index
8. background runtime controls, cancellation, and performance gates

This roadmap is intentionally benchmarked against at least three external toolchain families:

1. C++: `clang` / `clangd`
2. Python: CPython PEG parser plus `parso` / `pyright`-style edit-time tooling
3. Rust: `rustc` / `rust-analyzer`

The goal is not to clone any one toolchain exactly. The goal is to adopt the strongest proven ideas from all three:

- C++ contributes compiler-truth reuse, background indexing, and workspace navigation discipline
- Python contributes tolerant edit-time parsing and best-effort semantic service under broken code
- Rust contributes query-driven semantics, stable intermediate representations, and high-quality completion architecture

This plan freezes the implementation order for milestones `M11-M19`.

---

## 2. Current Gaps

### 2.1 Infrastructure gaps

Current behavior:

1. [`src/StyioLSP/Server.cpp`](../../src/StyioLSP/Server.cpp) still degrades `didChange` to a simplified whole-document path in important cases.
2. [`src/StyioIDE/VFS.cpp`](../../src/StyioIDE/VFS.cpp) does not yet expose a first-class multi-edit delta boundary to all consumers.
3. [`src/StyioIDE/SemDB.cpp`](../../src/StyioIDE/SemDB.cpp) still centers the public contract around a coarse `IdeSnapshot`.

Consequence:

- syntax and semantic invalidation remain broader than necessary
- cache hit behavior is hard to observe and optimize
- future item-level reuse has no stable dependency graph to attach to

### 2.2 Semantic model gaps

Current behavior:

1. [`src/StyioIDE/HIR.cpp`](../../src/StyioIDE/HIR.cpp) is still practical rather than fully canonical.
2. Definition/reference behavior is not yet backed by a complete scope graph.
3. Type information is not cached at the granularity needed for high-quality completion.

Consequence:

- unaffected items are harder to preserve across edits
- local shadowing/import resolution rules are harder to make precise
- receiver-sensitive/member-aware completion quality still trails the baseline set by mature C++, Python, and Rust IDE stacks

### 2.3 Runtime and quality gaps

Current behavior:

1. background work, cancellation, and debounce are minimal
2. workspace index layers are still shallow
3. fuzz/perf/drift gates are incomplete

Consequence:

- stale requests can waste work
- cross-file navigation and workspace symbol behavior will not scale cleanly
- performance regressions can slip in before they are visible to users

---

## 3. Architectural Invariants

These decisions are fixed for the entire roadmap:

| Area | Decision |
|------|----------|
| Repository layout | Language truth and IDE truth stay in the main `styio` repository |
| Parser layering | Tree-sitter-style syntax parser serves editing structure; Nightly parser remains semantic/compiler truth |
| Snapshot identity | Every semantic/cache boundary keys off `FileId` and `SnapshotId` |
| HIR boundary | IDE features consume HIR, scope graph, and query results rather than traversing AST classes directly |
| Completion | Parser supplies context and expected categories; semantic layers supply concrete candidates and ranking |
| Indexing | Open-file state, background index, and persistent index remain distinct layers |
| Runtime | Foreground latency has priority over background indexing and maintenance work |

Comparative baseline that remains in scope across the roadmap:

| Toolchain family | Reference tools | Styio lesson to adopt |
|------|----------|----------|
| C++ | `clang`, `clangd` | Reuse compiler truth where possible; separate open-file and background index responsibilities |
| Python | CPython PEG parser, `parso`, `pyright` | Keep edit-time parsing tolerant and semantic service best-effort under incomplete code |
| Rust | `rustc`, `rust-analyzer` | Use query-shaped semantics, stable intermediate forms, and completion built on semantic context rather than token guesses |

---

## 4. Pre-Implementation Decision Checklist

The roadmap is clear enough to implement, but these decisions should be frozen before work enters the corresponding milestone. Leaving them implicit will create avoidable rework.

| Decision Area | What Must Be Frozen | Recommended Baseline | Latest Freeze Point | Why It Matters |
|------|----------|----------|----------|----------|
| LSP document sync contract | Whether `textDocument/didChange` is treated as ordered incremental edits by default, and how full-sync clients fall back | Incremental sync is the primary path; full-text replacement remains a compatibility fallback | Before `M11` | This determines the public shape of `IdeService::did_change`, VFS update APIs, and all edit-path tests |
| Internal text coordinate model | How LSP `Position` maps to internal offsets | Keep UTF-16 line/character only at the LSP boundary; convert immediately to canonical internal UTF-8/byte offsets through one shared conversion path | Before `M11` | If this stays fuzzy, `TextEdit`, `TSInputEdit`, diagnostics, and navigation ranges will drift or break on non-ASCII text |
| Delta normalization and coalescing | How multiple edits are normalized, ordered, and optionally merged | Preserve LSP order; normalize against the current working text; only coalesce overlapping/adjacent edits after normalization | Before `M11` | Tree-sitter incremental reuse depends on correct edit coordinates; wrong coalescing will corrupt trees or over-invalidate syntax |
| Snapshot and semantic ID stability | What “stable” means for `SnapshotId`, `ItemId`, `ScopeId`, and `SymbolId` | `SnapshotId` changes every document transition; semantic IDs stay stable across unrelated in-session edits, but cross-restart stability is not required in the first version | Before `M13` | HIR identity, cache reuse, and index ownership all depend on a precise stability contract |
| Module and import model | File-to-module mapping and import-path resolution rules | One source file maps to one primary module; import resolution is project-aware; local scope beats imports, imports beat builtins unless syntax says otherwise | Before `M14` | Name resolution, cross-file definition, and workspace index behavior will be inconsistent without one module/import model |
| Builtin and capability metadata source | Where builtin symbols, methods, and capability flags come from | Keep one SSOT metadata layer for builtins and capability-bearing types; IDE consumers read it through semantic queries, not ad hoc tables | Before `M14` | Resolver, hover, member completion, and diagnostics all need the same builtin truth |
| IDE type-system scope for v1 | Which type features are required for IDE-quality semantics in this roadmap | First version must support signatures, receiver/member typing, expected argument types, and capability-based filtering; deeper generic/overload behavior can remain conservative | Before `M15` | Type inference work can balloon quickly; this keeps M15/M16 bounded to what completion and hover actually need |
| Completion ranking policy | How candidates are filtered and ordered across contexts | Exact prefix beats fuzzy; locals/params beat same-file top-levels; same-file top-levels beat imports; imports beat builtins; builtins beat keywords; keywords beat snippets | Before `M16` | Without a frozen ranking policy, completion quality changes become subjective and regression tests stay weak |
| Index freshness model | How open-file, background, and persistent index data interact | Unsaved open-file state always overrides background and persistent index data; background index reads project files from disk; persistent index only warms cold start | Before `M17` | Cross-file definition/references will disagree with open buffers if precedence is not explicit |
| Runtime scheduling model | How foreground requests, semantic diagnostics, and background indexing are prioritized | One foreground request lane plus lower-priority background work; semantic diagnostics are debounced; stale requests are dropped by snapshot/version guards | Before `M18` | Cancellation, debounce, and latency budgets cannot be implemented coherently without a fixed runtime model |
| Diagnostic merge and dedup policy | How syntax diagnostics, recovery diagnostics, and semantic diagnostics are combined | Publish syntax diagnostics immediately; semantic diagnostics are delayed; duplicate ranges/messages from recovery and semantic layers are deduplicated before publish | Before `M18` | Users will otherwise see duplicated or flickering diagnostics, especially under recovery mode |
| Performance measurement contract | How the roadmap’s latency budgets are measured and enforced | Freeze benchmark corpora, warm/hot conditions, and the measurement harness before gating M19 | Before `M19` | Latency budgets are meaningless if every run uses different inputs, cache states, or machine assumptions |

Any change to one of these decisions after implementation starts should update this plan first, then revise the affected milestone acceptance documents.

---

## 5. Decision Freeze Order

Not every decision above must be resolved immediately. The roadmap only needs the decisions that unblock the next stage to be frozen now.

| Priority | Decide When | Decision Areas | Why This Timing Is Correct |
|------|----------|----------|----------|
| `Now` | Before resuming implementation of `M11` | LSP document sync contract; internal text coordinate model; delta normalization and coalescing | These three decisions define the edit pipeline shape. Without them, `Server`, `IdeService`, `VFS`, `SyntaxParser`, and Tree-sitter reuse all risk immediate rework |
| `Next gate` | Before `M13` starts, but not required to begin `M11/M12` | Snapshot and semantic ID stability | `M11/M12` can land with document/snapshot IDs only. Stable semantic IDs become critical only once HIR identity enters the system |
| `Stage B gate` | Before `M14` starts | Module and import model; builtin and capability metadata source | Resolver and scope-graph work needs one coherent ownership model for files, imports, and builtins. Earlier milestones can proceed without freezing every semantic edge case |
| `Stage B gate` | Before `M15` starts | IDE type-system scope for v1 | Type inference work expands rapidly if the supported type surface is left open-ended. This decision should be frozen right before inference-query work begins |
| `Stage C gate` | Before `M16` starts | Completion ranking policy | Completion infrastructure can exist earlier, but completion quality and regression tests become unstable unless ranking is frozen before the upgrade milestone |
| `Stage C gate` | Before `M17` starts | Index freshness model | Workspace index merge rules only matter once open-file, background, and persistent layers are all active |
| `Stage D gate` | Before `M18` starts | Runtime scheduling model; diagnostic merge and dedup policy | Cancellation, debounce, and diagnostic publication semantics should be designed together, not improvised while runtime hardening is already underway |
| `Final gate` | Before `M19` starts | Performance measurement contract | The performance gates should be measured against a frozen harness and corpus, but they do not need to block earlier architectural work |

Recommended execution order for actual decision-making sessions:

1. Freeze the three `Now` decisions and then resume `M11`.
2. Freeze semantic ID stability before opening `M13`.
3. Freeze module/import and builtin metadata rules before opening `M14`.
4. Freeze IDE type-system scope before opening `M15`.
5. Freeze completion ranking before opening `M16`.
6. Freeze index freshness before opening `M17`.
7. Freeze runtime and diagnostic merge behavior before opening `M18`.
8. Freeze benchmark harness details before opening `M19`.

If a later-stage decision is pulled earlier for convenience, the plan should still preserve the same dependency direction.

---

## 6. Delivery Strategy

The roadmap is intentionally staged. Each stage only starts once the previous one stabilizes.

### Stage A — Incremental substrate

1. `M11` Multi-edit syntax path
2. `M12` File/offset query cache

Purpose:

- make edit deltas explicit
- make syntax and semantic recomputation observable

### Stage B — Stable semantic core

1. `M13` Stable HIR and item identity
2. `M14` Name resolution and scope graph
3. `M15` Type inference queries

Purpose:

- make semantic identity durable across edits
- make definition/reference/completion consume real semantic structures

### Stage C — IDE feature quality

1. `M16` Completion engine upgrade
2. `M17` Workspace index

Purpose:

- move completion quality from token heuristics toward type- and scope-driven ranking
- scale definition/references/workspace symbol beyond the current open-file path

### Stage D — Runtime hardening and closure

1. `M18` IDE runtime
2. `M19` Quality and performance closure

Purpose:

- keep foreground latency stable under load
- lock in test, fuzz, and performance gates before expanding feature surface again

---

## 7. Milestone Roadmap

### 5.1 M11 — Multi-Edit Incremental Syntax

Frozen acceptance:

- [`../milestones/2026-04-15/M11-MultiEditIncrementalSyntax.md`](../milestones/2026-04-15/M11-MultiEditIncrementalSyntax.md)

Goal:

1. preserve ordered LSP edits
2. let VFS own canonical text application
3. let Tree-sitter consume structured multi-edit deltas

Key outputs:

- `TextEdit` and `DocumentDelta`
- `didChange -> VFS -> SyntaxParser` delta path
- multi-edit incremental reuse and fallback hierarchy

### 5.2 M12 — Semantic Query Cache

Frozen acceptance:

- [`../milestones/2026-04-15/M12-SemanticQueryCache.md`](../milestones/2026-04-15/M12-SemanticQueryCache.md)

Goal:

1. replace coarse `IdeSnapshot` recomputation with explicit queries
2. split file-level and offset-level caches
3. formalize invalidation across snapshot transitions

Key outputs:

- `FileVersionKey` and `OffsetKey`
- explicit query families
- cache instrumentation and invalidation rules

### 5.3 M13 — Stable HIR and Item Identity

Frozen acceptance:

- [`../milestones/2026-04-15/M13-StableHIRAndItemIdentity.md`](../milestones/2026-04-15/M13-StableHIRAndItemIdentity.md)

Goal:

1. lower semantic truth into a stable HIR instead of token-driven summaries
2. give top-level items, scopes, and locals durable IDs
3. let unaffected items survive edits without semantic rebinding

Key outputs:

- canonical `ModuleId`, `ItemId`, `ScopeId`, `TypeId`, `SymbolId`
- AST-to-HIR lowering for functions, imports, locals, blocks, and resources
- stable identity rules for unchanged items

### 5.4 M14 — Name Resolution and Scope Graph

Frozen acceptance:

- [`../milestones/2026-04-15/M14-NameResolutionAndScopeGraph.md`](../milestones/2026-04-15/M14-NameResolutionAndScopeGraph.md)

Goal:

1. resolve names through explicit lexical and module scopes
2. model shadowing, imports, builtins, and cross-file lookup coherently
3. remove definition/reference behavior that still depends on text matching

Key outputs:

- scope graph
- import and builtin resolver
- symbol-to-definition/reference mapping

### 5.5 M15 — Type Inference Queries

Frozen acceptance:

- [`../milestones/2026-04-15/M15-TypeInferenceQueries.md`](../milestones/2026-04-15/M15-TypeInferenceQueries.md)

Goal:

1. move semantic caching below whole-file granularity
2. split signature inference from body inference
3. invalidate only the edited item wherever possible

Key outputs:

- per-item inference queries
- body hashes or equivalent invalidation keys
- receiver type and expected-type data consumable by completion and hover

### 5.6 M16 — Completion Engine Upgrade

Frozen acceptance:

- [`../milestones/2026-04-15/M16-CompletionEngineUpgrade.md`](../milestones/2026-04-15/M16-CompletionEngineUpgrade.md)

Goal:

1. make completion depend on syntax position, scope, receiver type, and call-argument context
2. rank locals/imports/builtins/snippets consistently
3. keep best-effort completion working under syntax errors

Key outputs:

- richer `CompletionContext`
- receiver-aware/member-aware ranking
- type-position and call-site filtering

### 5.7 M17 — Workspace Index

Frozen acceptance:

- [`../milestones/2026-04-15/M17-WorkspaceIndex.md`](../milestones/2026-04-15/M17-WorkspaceIndex.md)

Goal:

1. add explicit open-file, background, and persistent index layers
2. accelerate workspace symbol, cross-file definition, and references
3. keep index freshness and foreground latency separate

Key outputs:

- index schemas and merge policy
- background indexing queue
- persisted symbol/reference store

### 5.8 M18 — IDE Runtime

Frozen acceptance:

- [`../milestones/2026-04-15/M18-IDERuntime.md`](../milestones/2026-04-15/M18-IDERuntime.md)

Goal:

1. debounce semantic diagnostics
2. cancel stale foreground requests
3. prioritize visible document work over background maintenance

Key outputs:

- request cancellation/version guards
- background task scheduling and priorities
- runtime counters and latency instrumentation

### 5.9 M19 — Quality and Performance Closure

Frozen acceptance:

- [`../milestones/2026-04-15/M19-QualityAndPerformanceClosure.md`](../milestones/2026-04-15/M19-QualityAndPerformanceClosure.md)

Goal:

1. freeze corpus, fuzz, and performance gates
2. keep Tree-sitter and Nightly parser structure drift visible
3. prevent further feature expansion until latency and stability targets hold

Key outputs:

- syntax/semantic drift corpus
- fuzz targets for syntax, completion, and LSP sync
- benchmark and regression harness

---

## 8. Dependency Order

The implementation order is fixed:

`M11 -> M12 -> M13 -> M14 -> M15 -> M16 -> M17 -> M18 -> M19`

Rationale:

1. `M11` makes edit boundaries explicit
2. `M12` makes query boundaries explicit
3. `M13-M15` make semantic identity, resolution, and inference stable enough for high-quality IDE features
4. `M16` depends on those semantic foundations for ranking and filtering
5. `M17` scales the same semantic truth across the workspace
6. `M18-M19` harden runtime behavior and keep the system measurable

Do not reorder this sequence without updating this plan first and then revising the frozen milestone batch.

---

## 9. Required Public Shapes

The following internal/public boundaries remain fixed across the roadmap:

```cpp
struct FileVersionKey { FileId file_id; SnapshotId snapshot_id; };
struct OffsetKey { FileId file_id; SnapshotId snapshot_id; std::size_t offset; };

struct CompletionContext {
  FileId file_id;
  SnapshotId snapshot_id;
  std::size_t offset;
  std::string prefix;
  PositionKind position_kind;
  TokenSet expected_tokens;
  CategorySet expected_categories;
  ScopeId scope_id;
  TypeId receiver_type_id;
};
```

Required query families by the end of this plan:

1. `syntax_tree`
2. `semantic_summary`
3. `hir_module`
4. `scope_graph`
5. `resolve_name`
6. `infer_type`
7. `document_symbols`
8. `semantic_tokens`
9. `completion_context`
10. `completion`
11. `hover`
12. `definition`
13. `references`
14. `workspace_symbols`

---

## 10. Out of Scope

Still out of scope for this roadmap:

1. browser-side WASM IDE execution as the primary path
2. remote multi-tenant semantic service
3. rename/code action/inlay hint as first-class deliverables before M19
4. multi-root workspace semantics
5. collaborative editing conflict resolution

Those may be planned later, but only after the current roadmap is complete.

---

## 11. Test and Performance Gates

Each milestone has its own frozen acceptance, but the roadmap is not complete unless these top-level gates hold by M19:

1. 5k-line file, hot incremental syntax parse: `p95 <= 10ms`
2. 5k-line file, hot completion: `p95 <= 50ms`
3. 5k-line file, hot hover/definition: `p95 <= 80ms`
4. current repository scale, first background index: `<= 5s`
5. no known parser drift on the frozen syntax corpus without an explicit waiver
6. fuzz targets for syntax, completion, and LSP sync run clean in CI

Feature work beyond this roadmap should not start until these gates are either green or explicitly waived in a new plan.
