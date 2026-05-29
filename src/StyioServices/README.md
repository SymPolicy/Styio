# StyioServices

**Purpose:** Provide the source-level entrypoint for externally consumable Styio language services.

**Last updated:** 2026-05-30

## Scope

`StyioServices` contains reusable language-facing services for package managers, IDEs, editors, CI systems, and other external tools. It is not a consumer-specific adapter layer. Consumer names such as `spio` or Vityo may use these services, and first-party projects may bind to richer convenience payloads, but the facts exposed here remain shared service facts rather than separate product-local grammar, diagnostic, or semantic authorities.

## Modules

1. [StyioCLI](./StyioCLI/README.md) exposes command-line service helpers such as authoritative syntax-only JSON checking and source context.
2. [StyioConfig](./StyioConfig/README.md) owns machine-readable compiler handoff contracts and source-build metadata.
3. [StyioIDE](./StyioIDE/README.md) owns in-process editor services: VFS, syntax snapshots, HIR, SemDB, indexing, completion, hover, definition, references, symbols, semantic tokens, and runtime scheduling counters.
4. [StyioLSP](./StyioLSP/README.md) owns the stdio Language Server Protocol surface built on `StyioIDE`.

The complete service inventory is [MANIFEST.md](./MANIFEST.md).

## Build Targets

1. `styio_cli_contract_core` builds `StyioCLI` and `StyioConfig` service code used by the full `styio` binary.
2. `styio_ide_core` builds the embeddable IDE service library.
3. `styio_lspd` builds the standalone LSP daemon.

## Diagnostic Contract

All public service diagnostics use the shared taxonomy in [DiagnosticContract.hpp](./DiagnosticContract.hpp). Public codes follow `STYIO_<PHASE>_<ERROR_FAMILY>`, carry a stable public phase, and keep process exit codes coarse. CLI JSONL sema/type/native diagnostics include feature-owned refinements such as `STYIO_SEMA_IMMUTABLE_BINDING`, `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY`, `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH`, `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`, `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`, `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE`, `STYIO_NATIVE_SOURCE_READ_FAILED`, `STYIO_NATIVE_SIGNATURE_NOT_FOUND`, and `STYIO_NATIVE_UNSUPPORTED_SIGNATURE` when a focused compiler family owns the condition. IDE and LSP diagnostics preserve compiler/service codes when they come from compiler-owned facts, and use service/editor codes for editor-only interaction diagnostics.

## Use

For CLI users:

```bash
styio check --syntax --json --file path/to/file.styio
styio --machine-info=json
styio --source-build-info=json
styio --compile-plan path/to/compile-plan.json
```

For embedders:

```cpp
#include "StyioServices/StyioIDE/Service.hpp"

styio::ide::IdeService service;
service.initialize("file:///workspace");
```

For editors:

```bash
styio_lspd
```

## Parser Authority

The hand-written nightly compiler parser is the only authority for accepted Styio grammar. IDE, LSP, package-manager, and external validation consumers must route syntax validity through `StyioCLI` or compiler-owned parser services instead of maintaining a separate accepted grammar.

## First-Party Adapters

Vityo and `spio` may use direct C++ APIs, CLI JSON/JSONL contracts, future FFI facades, or hosted service payloads when that is more ergonomic than public LSP. Those adapters must still preserve parser evidence, grammar version, diagnostic identity, capability state, document revision, and workspace/config identity from the shared StyioServices contracts.
