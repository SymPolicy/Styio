# Styio IDE C++ API

**Purpose:** Show how to embed Styio's IDE components directly from C++, from the high-level `IdeService` entrypoint down to the edit-time `SyntaxParser`.

**Last updated:** 2026-04-22

## High-Level Service

Use `styio::ide::IdeService` when your host wants one façade over VFS, syntax, HIR, semdb, indexing, and IDE DTOs.

```cpp
#include "StyioIDE/Service.hpp"

styio::ide::IdeService service;
service.initialize("file:///abs/workspace");

const std::string uri = "file:///abs/workspace/sample.styio";
service.did_open(uri, "# add := (a: i32, b: i32) => a + b\nresult: i32 := ad\n", 1);

auto items = service.completion(uri, styio::ide::Position{1, 16});
auto hover = service.hover(uri, styio::ide::Position{1, 16});
auto defs = service.definition(uri, styio::ide::Position{1, 16});
auto refs = service.references(uri, styio::ide::Position{1, 16});
auto symbols = service.document_symbols(uri);
auto semantic_tokens = service.semantic_tokens(uri);
auto context = service.completion_context(uri, styio::ide::Position{1, 16});
```

Public façade methods currently available in [../../../src/StyioIDE/Service.hpp](../../../src/StyioIDE/Service.hpp):

1. `initialize`
2. `did_open`
3. `did_change`
4. `did_close`
5. `completion`
6. `hover`
7. `definition`
8. `references`
9. `document_symbols`
10. `workspace_symbols`
11. `semantic_tokens`
12. `completion_context`
13. `snapshot_for_uri`

## Edit-Time Syntax Only

Use `styio::ide::SyntaxParser` when you only need a tolerant syntax snapshot and do not want the full semantic pipeline.

```cpp
#include "StyioIDE/Syntax.hpp"

styio::ide::DocumentSnapshot snapshot;
snapshot.file_id = 1;
snapshot.snapshot_id = 1;
snapshot.path = "memory://sample.styio";
snapshot.version = 1;
snapshot.buffer = styio::ide::TextBuffer{"# add := (a: i32, b: i32) => a + b\n"};

styio::ide::SyntaxParser parser;
auto syntax = parser.parse(snapshot);

auto kind = syntax.position_kind_at(0);
auto prefix = syntax.prefix_at(5);
auto node = syntax.node_at_offset(3);
```

The returned `SyntaxSnapshot` exposes:

1. `tokens`
2. `nodes`
3. `diagnostics`
4. `backend`
5. `reused_incremental_tree`
6. `matching_tokens`
7. `folding_ranges`
8. `position_kind_at`
9. `expected_tokens_at`
10. `expected_categories_at`
11. `prefix_at`
12. `scope_hint_at`
13. `node_path_at`
14. `node_at_offset`

`reused_incremental_tree` is `true` when the same `SyntaxParser` instance successfully reused an earlier Tree-sitter tree for the same document path.

## Semantic Bridge Only

Use `styio::ide::analyze_document` when you want Nightly semantic facts without the full IDE service.

```cpp
#include "StyioIDE/CompilerBridge.hpp"

auto summary = styio::ide::analyze_document("memory://sample.styio", source_text);
if (summary.used_recovery) {
  // The parse continued past at least one malformed statement.
}
```

`SemanticSummary` currently reports:

1. `parse_success`
2. `used_recovery`
3. `diagnostics`
4. `items`
5. `inferred_types`
6. `function_signatures`

`items` contains AST/analyzer-backed semantic item facts for module-level functions, imports, resources, and global bindings. Top-level `@import { ... }` declarations surface here as import items; import names are exposed in canonical slash form (`std/io`), even when the source used the compatibility dot spelling (`std.io`). HIR lowering uses these facts as semantic truth, then binds them to edit-time syntax ranges.

## HIR Identity

`styio::ide::HirModule` is the canonical IDE semantic layer below `SemanticDB`.

1. `HirItem` represents module-level semantic items and owns stable `ItemId`s.
2. `HirScope` represents module, item, and block scopes with stable `ScopeId`s.
3. `HirSymbol` represents name bindings for items, params, and locals; item-backed symbols carry `item_id`.
4. `HirIdentityStore` retains per-file item/scope/symbol identity across snapshot changes in one IDE session.

Identity is stable for unchanged semantic identities within one session. `ModuleId` maps to `FileId`; body edits and range shifts keep a same-name top-level item identity, while renames produce a new item identity.

`HirItem` also carries `signature_fingerprint` and `body_fingerprint`. For hash-style functions, the signature fingerprint covers the text before `=>` and the body fingerprint covers the text after `=>`; bindings use the assignment operator as the split point.

## SemanticDB Query Cache

`IdeService` callers do not manage cache state directly. Internally, `SemanticDB` resolves IDE requests through explicit query families:

File-level queries:

1. `syntax_tree(file, snapshot)`
2. `semantic_summary(file, snapshot)`
3. `hir_module(file, snapshot)`
4. `document_symbols(file, snapshot)`
5. `semantic_tokens(file, snapshot)`

Offset-level queries:

1. `completion_context(file, snapshot, offset)`
2. `completion(file, snapshot, offset)`
3. `hover(file, snapshot, offset)`
4. `definition(file, snapshot, offset)`
5. `references(file, snapshot, offset)`

Item and typed-site queries:

1. `type_signature(file, item_id, signature_fingerprint)`
2. `type_body(file, item_id, signature_fingerprint, body_fingerprint)`
3. `receiver_type(file, snapshot, offset)`
4. `expected_type(file, snapshot, offset)`

File queries are cached by `FileVersionKey(FileId, SnapshotId)`. Offset queries are cached by `OffsetKey(FileId, SnapshotId, offset)`. A document snapshot transition invalidates query state for that file; `did_close` also evicts open-file query state and the incremental syntax tree.

Type signature/body queries are item-local and may survive snapshot transitions. Receiver and expected-type queries are snapshot-local because their offsets and token context come from the current syntax snapshot.

`CompletionContext` exposes typed context for hosts that need to build richer UI. Direct member sites populate `receiver_type_id` and `receiver_type_name`; direct call arguments populate `expected_type_name`, `expected_param_name`, and `argument_index`.

## Completion Policy

Completion results are filtered before ranking:

1. `PositionKind::Type` returns type-shaped candidates.
2. `PositionKind::MemberAccess` returns property-shaped candidates.
3. General expression and statement positions suppress type-only and member-only candidates.

Ranking is deterministic:

1. exact prefix and prefix matches rank ahead of weaker text matches
2. visible locals and parameters rank ahead of same-file top-level symbols
3. same-file top-level symbols rank ahead of imported symbols
4. imported symbols rank ahead of builtins
5. builtins rank ahead of keywords
6. snippets rank last

Duplicate labels keep the highest-scoring visible candidate. This preserves shadowing: a parameter or local binding with the same name as a global/imported/builtin candidate is the one surfaced.

Receiver-aware completion currently uses conservative builtin capability metadata. List-like receivers expose `len`, `first`, and `last`; dict-like receivers expose `len`, `keys`, and `values`; string receivers expose `len`. Call-site completion boosts candidates whose `CompletionItem::type_name` matches the expected parameter type.

## Workspace Index

`SemanticDB` owns three workspace index layers:

1. `OpenFileIndex` contains fresh unsaved symbols and references for open buffers.
2. `BackgroundIndex` contains symbols and references from scanned workspace files on disk.
3. `PersistentIndex` contains workspace-scoped persisted top-level symbol metadata for warm start.

Merge precedence is deterministic: open-file entries override background and persistent entries for the same path; background entries override persistent entries; duplicate locations are normalized by path and range. `workspace_symbols` consumes all three layers. Cross-file definition can use the merged symbol index when a file has no explicit import constraint, and references merge precise open-file results with background-indexed reference hits.

`SemanticDB::query_stats()` and `SemanticDB::reset_query_stats()` expose per-query hit/miss counters for focused tests and local diagnostics. They are not part of the LSP protocol surface.

## Layer Boundaries

1. `SyntaxParser` owns edit-time CST and tolerant token spans.
2. Nightly parser + analyzer remain the semantic truth for `SemanticSummary`, with `ParseMode::Recovery` enabled for IDE usage.
3. `HirBuilder` lowers syntax plus semantic summary into the IDE-facing HIR.
4. `SemanticDB` owns file-level and offset-level IDE query caches.
5. `IdeService` is the recommended stable boundary for application code.

## Runtime

`IdeService` now owns the runtime scheduling boundary above `SemanticDB`.

1. `did_open` / `did_change` return syntax diagnostics immediately and queue a debounced semantic diagnostic publication for the latest visible snapshot.
2. `drain_semantic_diagnostics()` publishes the queued full diagnostic sets. Older queued work is dropped if the document snapshot/version has already advanced.
3. `begin_foreground_request(...)` returns a `ForegroundRequestTicket`. Guarded `completion`, `hover`, `definition`, and `references` overloads drop stale or canceled work instead of returning results for obsolete snapshots.
4. `cancel_request(...)` marks a foreground request as canceled; `RuntimeCounters` records the drop.
5. `schedule_background_index_refresh()` and `run_background_tasks()` provide a lower-priority background lane for reindex work.
6. `run_idle_tasks()` is the explicit idle slice: it drains queued semantic diagnostics first, then runs background index work within the supplied budget.
7. `runtime_counters()` exposes coarse counters and latency aggregates for stale drops, cancellations, debounce activity, foreground-yield events, and background task completion.

To keep the M17 workspace-index guarantees under the new debounce model, dirty open buffers are refreshed into `OpenFileIndex` lazily before `workspace_symbols` and cross-file definition queries.
