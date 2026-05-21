# StyioLSP

**Purpose:** Provide the editor-neutral Language Server Protocol service built on `StyioIDE`.

**Last updated:** 2026-05-21

## Use

Run the daemon over stdio:

```bash
styio_lspd
```

Editors should launch `styio_lspd` as an LSP server process and speak JSON-RPC over stdio.

For tests or embedded transports, use the C++ server directly:

```cpp
#include "StyioServices/StyioLSP/Server.hpp"

styio::lsp::Server server;
std::vector<styio::lsp::OutboundMessage> replies = server.handle(std::move(request));
```

## Available Functions

| Function or Type | Use |
|------------------|-----|
| `OutboundMessage` | Represents a JSON-RPC response or notification emitted by the server. |
| `Server::handle(...)` | Handles one decoded JSON request object and returns outbound messages. |
| `Server::drain_runtime()` | Drains pending semantic diagnostics and background work notifications. |
| `Server::drain_runtime(max_documents)` | Drains a bounded number of runtime diagnostic publications. |
| `Server::runtime_counters()` | Exposes underlying `StyioIDE` runtime counters for tests and diagnostics. |
| `Server::run(input, output)` | Runs the stdio framing loop for the LSP daemon. |

## Current LSP Surface

The server is backed by `styio::ide::IdeService` and currently supports document lifecycle, diagnostics, completion, hover, definition, references, document/workspace symbols, semantic tokens, runtime drain, and scheduling-related test hooks.

Published LSP diagnostics carry Styio diagnostic identity when available:

1. LSP `Diagnostic.code` is the shared `STYIO_<PHASE>_<ERROR_FAMILY>` code.
2. LSP `Diagnostic.data.phase` is the public Styio phase.
3. LSP `Diagnostic.source` remains `styio-compiler`, `styio-editor`, or `styio-lsp` depending on the origin.

See the full service inventory in [../MANIFEST.md](../MANIFEST.md).
