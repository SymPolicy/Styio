# StyioIDE

**Purpose:** Provide the embeddable C++ IDE service layer for Styio source editing, navigation, diagnostics, and workspace queries.

**Last updated:** 2026-05-14

## Use

Embed the facade when an IDE or editor host wants in-process services:

```cpp
#include "StyioServices/StyioIDE/Service.hpp"

styio::ide::IdeService service;
service.initialize("file:///workspace");
auto diagnostics = service.did_open("file:///workspace/main.styio", "x := 1\n", 1);
auto completions = service.completion("file:///workspace/main.styio", styio::ide::Position{0, 1});
```

Use lower-level components only when the host needs a narrower service:

```cpp
#include "StyioServices/StyioIDE/Syntax.hpp"
#include "StyioServices/StyioIDE/VFS.hpp"
```

## Available Functions

| Capability | Entry Point | Use |
|------------|-------------|-----|
| URI/path conversion | `path_from_uri`, `uri_from_path` | Normalize editor and filesystem identifiers. |
| Text position mapping | `TextBuffer::position_at`, `TextBuffer::offset_at` | Convert byte offsets and LSP-style positions. |
| Virtual documents | `VirtualFileSystem` | Open, update, incrementally edit, close, and snapshot documents. |
| Syntax parsing | `SyntaxParser::parse` | Produce tolerant or Tree-sitter syntax snapshots. |
| Syntax cache eviction | `SyntaxParser::drop_cached_file` | Clear incremental parser state for closed or invalidated files. |
| Compiler semantic facts | `analyze_document` | Reuse compiler parse/type facts for editor diagnostics and summaries. |
| HIR model | `HirBuilder::build` | Build item, scope, symbol, and reference models from syntax and semantic summaries. |
| Open-file indexing | `OpenFileIndex` | Query symbols and references from open documents. |
| Background indexing | `BackgroundIndex` | Query symbols and references from background workspace scans. |
| Persistent indexing | `PersistentIndex` | Save and load symbol summaries from cache storage. |
| Semantic snapshots | `SemanticDB::build_snapshot` | Build a combined document, syntax, semantic, HIR, and diagnostic view. |
| Diagnostics | `syntax_diagnostics_for`, `diagnostics_for` | Retrieve syntax-only or full IDE diagnostics. |
| Completion | `completion_context_at`, `complete_at`, `IdeService::completion` | Produce ranked completion items. |
| Hover | `hover_at`, `IdeService::hover` | Produce hover contents and ranges. |
| Definition | `definition_at`, `IdeService::definition` | Resolve symbol definitions. |
| References | `references_of`, `IdeService::references` | Resolve references to a symbol target. |
| Type queries | `type_signature_at`, `type_body_at`, `receiver_type_at`, `expected_type_at` | Query signature/body and contextual type information. |
| Symbols | `document_symbols`, `workspace_symbols` | List document and workspace symbols. |
| Semantic tokens | `semantic_tokens_for`, `IdeService::semantic_tokens` | Produce token streams for semantic highlighting. |
| Runtime scheduling | `begin_foreground_request`, `cancel_request`, `drain_semantic_diagnostics`, `run_idle_tasks` | Coordinate foreground requests, cancellation, semantic diagnostics, and background indexing. |
| Runtime counters | `runtime_counters`, `reset_runtime_counters` | Inspect and reset request and background-work counters. |

## Boundaries

`StyioIDE` is an in-process API. If a host needs protocol transport instead, use [../StyioLSP/README.md](../StyioLSP/README.md). If a host only needs syntax validity, use [../StyioCLI/README.md](../StyioCLI/README.md).

See the full service inventory in [../MANIFEST.md](../MANIFEST.md).

