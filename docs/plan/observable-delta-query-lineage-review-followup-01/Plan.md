# PLAN-007 styio_lspd terminates on uncaught filesystem_error when the IDE cache directory cannot be created

Phase: completed · Revision: unsealed

This document is a render-only projection of `Plan.json`. Edit `Plan.json`; never edit this file.

## Intent

**Goal**: On initialize, the IDE index cache unconditionally calls std::filesystem::create_directories for the per-root cache directory under the user cache root without catching std::filesystem::filesystem_error. When that cache root is not writable (read-only or sandboxed home), the daemon aborts via libc++abi before answering the initialize request, which surfaces as the styio_lspd_stdio_framing smoke test timing out with zero bytes.

**In scope**
- On initialize, the IDE index cache unconditionally calls std::filesystem::create_directories for the per-root cache directory under the user cache root without catching std::filesystem::filesystem_error. When that cache root is not writable (read-only or sandboxed home), the daemon aborts via libc++abi before answering the initialize request, which surfaces as the styio_lspd_stdio_framing smoke test timing out with zero bytes.
- src/StyioServices/StyioIDE/Index.cpp

**Out of scope**
- Changes unrelated to PLAN-007's confirmed repair outcome.

**Success**
- styio_lspd answers initialize, or exits with a deliberate attributed diagnostic, when the IDE cache directory cannot be created
- styio_lspd_stdio_framing passes in an environment with a read-only home directory

**Risk boundary**
- Do not change LSP protocol behavior, stdio framing bytes, or the per-root cache identity scheme
- Do not weaken the explicit environment-fallback and workspace-skip accounting rules for IDE project roots

## Decisions

Dossier status: not_required

No non-discoverable user decision was required.

### Observed repository facts

- On initialize, the IDE index cache unconditionally calls std::filesystem::create_directories for the per-root cache directory under the user cache root without catching std::filesystem::filesystem_error. When that cache root is not writable (read-only or sandboxed home), the daemon aborts via libc++abi before answering the initialize request, which surfaces as the styio_lspd_stdio_framing smoke test timing out with zero bytes. (source: src/StyioServices/StyioIDE/Index.cpp)
- The LSP daemon crashes at startup instead of degrading (for example disabling the on-disk index cache) or failing with a clean diagnostic; the transport smoke test fails for purely environmental reasons, which can mask real regressions in sandboxed CI. (source: src/StyioServices/StyioIDE/Index.cpp)
- A manual initialize handshake against the built styio_lspd with a fresh workspace root under a read-only home prints libc++abi terminating due to uncaught exception filesystem_error in create_directories (Operation not permitted); the same test passes unchanged once the home directory is writable. (source: src/StyioServices/StyioIDE/Index.cpp)
- The repair in PR #42 makes IDE index-cache directory creation and symbols.json persistence best-effort: an unavailable cache returns without changing the LSP protocol or terminating the daemon. (source: src/StyioServices/StyioIDE/Index.cpp)
- StyioWorkspaceIndex.PersistentIndexSaveSkipsInvalidCachePath covers an invalid cache parent, while styio_lspd_stdio_framing verifies a byte-exact initialize response when the cache root is unavailable. (source: tests/ide/styio_ide_test.cpp and tests/lsp_stdio_framing_test.py)
- The nightly merge commit containing PR #42 passed smoke, Linux, macOS, golden-standard, hygiene, audit, and Windows smoke checks. (source: nightly CI for merge commit 5d4ebe497b1741fa83573b113222c89dc584a7)

## Requirements

None recorded yet.

## Architecture

Summary: Make IDE index cache persistence fail-safe while preserving the existing LSP response bytes and cache identity.
Notes:
- Cache persistence is best-effort: a directory or symbols.json write failure must not terminate styio_lspd or alter its stdio protocol.
- The repair keeps the existing per-root cache identity and workspace fallback accounting unchanged.

## Tasks

### TASK-001 Make IDE cache persistence best-effort

Outcome: Guard cache directory creation and symbols.json writes, then prove invalid-cache initialization and persistence behavior with focused IDE and stdio tests.

Scope in:
- src/StyioServices/StyioIDE/Index.cpp
- tests/ide/styio_ide_test.cpp
- tests/lsp_stdio_framing_test.py

Scope out:
- LSP protocol changes, cache identity changes, and unrelated IDE behavior

## Full regression

Run inside the sole Reviewer session after every repair is integrated.

- `cmake --build build/default --target styio_lspd styio_ide_test -j2`
- `ctest --test-dir build/default -L ide --output-on-failure --no-tests=error`
- `python3 tests/lsp_stdio_framing_test.py`
- `nightly CI for merge commit 5d4ebe497b1741fa83573b113222c89dc584a7f9`
- paths: `src/StyioServices/StyioIDE/Index.cpp`, `tests/ide/styio_ide_test.cpp`, `tests/lsp_stdio_framing_test.py`
