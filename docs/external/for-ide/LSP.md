# Styio LSP Usage

**Purpose:** Define how IDE hosts should launch and talk to `styio_lspd`, and record the currently supported request and notification surface.

**Last updated:** 2026-04-22

## Transport

1. Binary: `build/default/bin/styio_lspd`
2. Protocol: LSP 3.17 over stdio
3. Lifetime: one long-lived local daemon per workspace root

## Supported Methods

1. `initialize`
2. `textDocument/didOpen`
3. `textDocument/didChange`
4. `textDocument/didClose`
5. `textDocument/completion`
6. `textDocument/hover`
7. `textDocument/definition`
8. `textDocument/references`
9. `textDocument/documentSymbol`
10. `workspace/symbol`
11. `textDocument/semanticTokens/full`
12. `textDocument/publishDiagnostics` notification
13. `$/cancelRequest`

## Startup Sequence

1. Launch `styio_lspd` with the workspace root available to the client.
2. Send `initialize` with `rootUri`.
3. Open editor buffers via `didOpen`.
4. Forward buffer changes via `didChange`.
5. Query completion, hover, definition, references, symbols, and semantic tokens against the open document state.

## Minimal Message Flow

```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"rootUri":"file:///abs/workspace"}}
{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///abs/workspace/sample.styio","version":1,"text":"# add := (a: i32, b: i32) => a + b\nresult: i32 := ad\n"}}}
{"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///abs/workspace/sample.styio"},"position":{"line":1,"character":16}}}
```

The current completion pipeline is:

`cursor -> VFS snapshot -> syntax position kind -> HIR + semdb -> typed context -> builtin/index merge -> ranked completion items`

Typed context currently covers direct member receivers and direct call-site argument expectations. LSP clients consume this through completion and hover behavior; the lower-level C++ `IdeService::completion_context` API exposes the raw context for in-process hosts.

Completion ordering follows the IDE policy: visible locals and parameters, same-file top-level symbols, imports, builtins, keywords, then snippets. Type/member positions filter by candidate shape, and member completion is receiver-aware for the builtin capability set.

Explicit imports come from top-level `@import { ... }` declarations. Source accepts both native slash paths (`styio/mod`) and compatibility dot paths (`styio.mod`), but semantic import facts are canonicalized to slash form before completion, definition, hover, and references consume them. Failed explicit imports stay unresolved instead of falling through to unrelated workspace symbols.

`workspace/symbol` reads the merged workspace index. Unsaved open buffers have priority over background-indexed disk files, and background entries have priority over persisted warm-start entries.

## Document Sync Contract

1. Incremental `textDocument/didChange` is the primary path.
2. Ranged `contentChanges` are applied in the original LSP order.
3. Full-document sync remains a compatibility fallback and is not optimized in the current M11 slice.
4. LSP UTF-16 `line/character` positions are converted at the server boundary; internal IDE layers use UTF-8 byte offsets.
5. Invalid or unsafe incremental ranges trigger full-document resynchronization rather than preserving partially applied edits.

## Diagnostics Semantics

1. `didOpen` / `didChange` publish syntax diagnostics immediately from the edit-time syntax snapshot.
2. Semantic diagnostics come from the Nightly parser/analyzer bridge and are queued behind a debounce boundary.
3. Debounced semantic publication replaces the earlier syntax-only list with the full merged diagnostic set for the latest visible snapshot.
4. Stale semantic runs are dropped by snapshot/version guards instead of being published.
5. In recovery mode, malformed statements are reported while later statements in the same file can still contribute hover, completion, and symbol data.

## Current Limits

1. The server is local-only and single-workspace for now.
2. `rename`, `codeAction`, and `inlayHint` are intentionally not implemented yet.
3. Debounced semantic publication is request-driven in the stdio loop: `Server::run()` drains runtime diagnostics after each processed request.
4. `workspace/didChangeWatchedFiles` schedules background reindex work; because the stdio runtime has no separate idle thread, `Server::run()` advances one background task as a request-driven fallback only after foreground responses and semantic diagnostic drains are clear. Embedders can call `IdeService::run_idle_tasks()` for the same semantic-first idle slice.
5. Stale foreground and semantic work is guarded by snapshot/version checks and counted instead of being published after a newer visible snapshot.
