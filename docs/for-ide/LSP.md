# Styio LSP Usage

**Purpose:** Define how IDE hosts should launch and talk to `styio_lspd`, and record the currently supported request and notification surface.

**Last updated:** 2026-04-14

## Transport

1. Binary: `build-codex/bin/styio_lspd`
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

`cursor -> VFS snapshot -> syntax position kind -> HIR + semdb -> builtin/index merge -> ranked completion items`

## Current Limits

1. The server is local-only and single-workspace for now.
2. `rename`, `codeAction`, and `inlayHint` are intentionally not implemented yet.
3. Diagnostics are currently recomputed on open/change without a debounce worker.
