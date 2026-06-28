# ADR-0121: LSP Windows Stdio Binary Mode

**Purpose:** Record the decision to force `styio_lspd` standard input and standard output into binary mode on Windows so LSP framing stays byte-exact.

**Last updated:** 2026-06-28

## Status

Accepted

## Context

`styio_lspd` speaks LSP 3.17 over stdio. LSP framing requires byte-exact
headers such as `Content-Length: N\r\n\r\n`.

On Windows, the C runtime defaults standard streams to text mode. Writing
explicit `\r\n` through a text-mode `std::cout` can expand the line ending into
`\r\r\n`. A permissive custom wire test can tolerate this, but the standard VS
Code `vscode-languageclient` stack waits for exact LSP framing and can hang
during `initialize`.

This was observed while validating the Styio VS Code extension against a real
VS Code installation: the daemon process started, but the language client did
not complete initialization.

## Decision

At `styio_lspd` process startup, set `stdin` and `stdout` to binary mode on
Windows before entering `Server::run(std::cin, std::cout)`. If either stream
cannot be switched to binary mode, fail fast before accepting LSP traffic.

Non-Windows platforms keep the existing behavior.

## Alternatives

1. Change the server writer to emit `\n\n` on Windows.
   - Rejected because LSP specifies CRLF framing and callers should not depend
     on platform text translation.
2. Make every client accept `\r\r\n\r\r\n`.
   - Rejected because production clients such as VS Code use their own LSP
     transport readers. The server must produce standard framing.
3. Move all framing to platform-specific low-level writes.
   - Rejected for now because binary mode preserves the current stream-based
     implementation with minimal risk.

## Consequences

Positive:

1. VS Code `vscode-languageclient` can parse `styio_lspd` responses on Windows.
2. LSP wire bytes become consistent across Windows, Linux, and macOS.
3. Existing `Server::run` tests and stream-based implementation remain intact.
4. `styio_lspd_stdio_framing` starts the real daemon executable and verifies
   the first LSP response uses byte-exact `\r\n\r\n` framing.

Negative:

1. Windows startup now depends on `_setmode`, `_fileno`, and `_O_BINARY` from
   the Microsoft C runtime headers.
2. If future code writes human-facing text to stdout before LSP starts, it will
   also be written in binary mode. `styio_lspd` should keep stdout reserved for
   LSP messages and use stderr for diagnostics.

## Validation

1. `tests/lsp_stdio_framing_test.py` launches the built `styio_lspd` process,
   sends an `initialize` request over stdin, reads raw stdout bytes, and rejects
   both `\r\r\n\r\r\n` and `\n\n` response boundaries.
2. `tests/CMakeLists.txt` registers that script as the CTest
   `styio_lspd_stdio_framing` with labels `ide;lsp;transport;smoke`.
