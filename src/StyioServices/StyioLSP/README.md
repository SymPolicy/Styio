# StyioLSP

**Purpose:** Provide the editor-neutral Language Server Protocol service built on `StyioIDE`.

**Last updated:** 2026-05-14

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

See the full service inventory in [../MANIFEST.md](../MANIFEST.md).

