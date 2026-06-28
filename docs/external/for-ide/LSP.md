# Styio LSP Usage

**Purpose:** Define how IDE hosts should launch and talk to `styio_lspd`, and record the currently supported request and notification surface.

**Last updated:** 2026-06-28

## Transport

1. Binary: `build/default/bin/styio_lspd`
2. Protocol: LSP 3.17 over stdio
3. Lifetime: one long-lived local daemon per selected workspace root
4. Windows stdio must remain byte-exact. `styio_lspd` sets stdin/stdout to binary mode before entering the LSP loop so `Content-Length: N\r\n\r\n` headers are not expanded to `\r\r\n\r\r\n` by the C runtime. Inbound ASCII case variants of `Content-Length` are accepted, but outbound frames use canonical spelling. See
   `docs/adr/ADR-0121-lsp-windows-stdio-binary-mode.md`.

## Supported Methods

1. `initialize`
2. `textDocument/didOpen`
3. `textDocument/didChange`
4. `textDocument/didClose`
5. `textDocument/completion`
6. `textDocument/hover`
7. `textDocument/definition`
8. `textDocument/references`
9. `textDocument/rename`
10. `textDocument/codeAction`
11. `textDocument/inlayHint`
12. `textDocument/documentSymbol`
13. `workspace/symbol`
14. `textDocument/semanticTokens/full`
15. `textDocument/publishDiagnostics` notification
16. `$/cancelRequest`

## Startup Sequence

1. Launch `styio_lspd` with the workspace root available to the client.
2. Send `initialize` with `rootUri` as a JSON-RPC request with an `id`. If `workspaceFolders` are supplied, the server records them, selects one active workspace, and ignores extra folders explicitly. Notification-shaped initialize messages are treated as malformed defensive input: the server may refresh internal workspace state, but it does not emit a response without a request id.
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

The `initialize` response now records the selected single-workspace state in `experimental.styio.workspaceState` and advertises `capabilities.workspace.workspaceFolders.supported = false` so multi-root support stays fail-closed rather than implicit.

## Document Sync Contract

1. Incremental `textDocument/didChange` is the primary path.
2. Ranged `contentChanges` are applied in the original LSP order.
3. Full-document sync remains a compatibility fallback and is not optimized in the current native-interop slice.
4. LSP UTF-16 `line/character` positions are converted at the server boundary; internal IDE layers use UTF-8 byte offsets. Semantic-token delta positions and lengths are also emitted in UTF-16 units.
5. Invalid or unsafe incremental ranges trigger full-document resynchronization rather than preserving partially applied edits.

## Diagnostics Semantics

1. `didOpen` / `didChange` publish edit-time syntax snapshot diagnostics immediately. These diagnostics support editor interaction but do not define accepted Styio grammar.
2. Semantic diagnostics come from the authoritative nightly parser/analyzer bridge and are queued behind a debounce boundary.
3. Debounced semantic publication replaces the earlier syntax-only list with the full merged diagnostic set for the latest visible snapshot.
4. Stale semantic runs are dropped by snapshot/version guards instead of being published.
5. Malformed source does not publish recovered later hover, completion, symbol, or type facts from the compiler semantic bridge.

## Rename Readiness

`textDocument/rename` is now part of the supported method list, but it stays conservative:

1. the server only returns a `WorkspaceEdit` when the cursor resolves to a compiler-owned symbol identity;
2. builtin, string, comment, unresolved, and other identity-free positions return `null`;
3. the current workspace state must be fresh enough that pending semantic diagnostics and background index work are drained first; and
4. every rename edit is rebuilt from the current snapshot text for the resolved target and its references.

The implementation keeps these proof points in view for future hardening:

1. semantic identity is compiler-owned and stable across open buffers, background indexes, and persisted warm-start entries;
2. stale foreground, semantic, and background-index work cannot publish or apply edits for an older snapshot;
3. workspace symbol/index facts agree with definition and references for the same identity;
4. diagnostics publication remains semantic-first and does not hide rename blockers behind syntax-only facts; and
5. rename fixtures cover freshness, workspace index identity, stale publication suppression, and malformed-source rejection.

The initial public checkpoint is covered by `StyioLspServer.RenameIsCapabilityGatedAndUsesResolvedSymbolIdentity`; future rename broadening should keep using parallel evidence lanes for identity, freshness, workspace index, diagnostics publication, and fixture coverage before relaxing any fail-closed case.

## Inlay Hints

`textDocument/inlayHint` is supported for call-argument parameter names. The server only emits a hint when the callee resolves, the argument slot is compiler-recognized, the parameter name comes from current semantic facts, and semantic/background work is already drained. Unresolved calls, malformed ranges, incomplete signatures, and stale workspace state return an empty list.

`codeAction` is implemented as a conservative, fail-closed action lane: the server returns edit quick-fixes for `unterminated block comment`, `unterminated string literal`, and exact-range `unmatched closing token` editor-syntax diagnostics, returns disabled `quickfix` suggestions for editor-syntax diagnostics it cannot safely auto-fix, and otherwise returns an empty action list.

## Current Limits

1. The server is local-only and single-workspace for now. `initialize` records the requested `rootUri`, supplied `workspaceFolders`, and the selected active root explicitly, then ignores any extra folders.
2. `codeAction` is conservative: only the `unterminated block comment`, `unterminated string literal`, and exact-range `unmatched closing token` editor-syntax diagnostics have edit-producing quick-fixes; unsupported editor-syntax diagnostics remain disabled quick-fix explanations, and all other diagnostics still return an empty action list.
3. `inlayHint` is freshness-gated: semantic/background work must already be drained before the server emits call-argument parameter hints.
4. Debounced semantic publication is request-driven in the stdio loop: `Server::run()` drains runtime diagnostics after each processed request.
5. `workspace/didChangeWatchedFiles` schedules background reindex work only for changed `.styio` file URIs under the selected workspace root. Open files, non-Styio files, out-of-workspace files, duplicate notifications, and empty `changes` arrays do not enqueue work. Because the stdio runtime has no separate idle thread, `Server::run()` advances one background task as a request-driven fallback only after foreground responses and semantic diagnostic drains are clear. Embedders can call `IdeService::run_idle_tasks()` for the same semantic-first idle slice.
6. Stale foreground and semantic work is guarded by snapshot/version checks and counted instead of being published after a newer visible snapshot.
