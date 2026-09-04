# PLAN-007 styio_lspd terminates on uncaught filesystem_error when the IDE cache directory cannot be created

Phase: draft · Revision: unsealed

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
- PLAN-005 excludes IDE SemanticDB caches and any IDE or LSP behavior change (REQ-014 requires IDE and LSP behavior to remain unchanged), this delivery makes no IDE or LSP source modification, and the defect predates the Plan. (source: observable-delta-query-lineage/Plan.json)

## Requirements

None recorded yet.

## Architecture



- none

## Tasks

None recorded yet.

## Full regression

Run inside the sole Reviewer session after every repair is integrated.

- `none`
- paths: none
