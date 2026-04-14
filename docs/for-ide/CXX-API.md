# Styio IDE C++ API

**Purpose:** Show how to embed Styio's IDE components directly from C++, from the high-level `IdeService` entrypoint down to the edit-time `SyntaxParser`.

**Last updated:** 2026-04-14

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
```

Public façade methods currently available in [../../src/StyioIDE/Service.hpp](../../src/StyioIDE/Service.hpp):

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
12. `snapshot_for_uri`

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
4. `matching_tokens`
5. `folding_ranges`
6. `position_kind_at`
7. `expected_tokens_at`
8. `expected_categories_at`
9. `prefix_at`
10. `scope_hint_at`
11. `node_path_at`
12. `node_at_offset`

## Layer Boundaries

1. `SyntaxParser` owns edit-time CST and tolerant token spans.
2. Nightly parser + analyzer remain the semantic truth for `SemanticSummary`.
3. `HirBuilder` lowers syntax plus semantic summary into the IDE-facing HIR.
4. `IdeService` is the recommended stable boundary for application code.
