# StyioServices

**Purpose:** Provide the source-level entrypoint for externally consumable Styio language services.

**Last updated:** 2026-05-14

## Scope

`StyioServices` contains reusable language-facing services for package managers, IDEs, editors, CI systems, and other external tools. It is not a consumer-specific adapter layer. Consumer names such as `spio` or Vityo may use these services, but the service contracts here must stay general enough for other tools.

## Modules

1. [StyioCLI](./StyioCLI/README.md) exposes command-line service helpers such as syntax-only JSON checking.
2. [StyioConfig](./StyioConfig/README.md) owns machine-readable compiler handoff contracts and source-build metadata.
3. [StyioIDE](./StyioIDE/README.md) owns in-process editor services: VFS, syntax snapshots, HIR, SemDB, indexing, completion, hover, definition, references, symbols, semantic tokens, and runtime scheduling counters.
4. [StyioLSP](./StyioLSP/README.md) owns the stdio Language Server Protocol surface built on `StyioIDE`.

The complete service inventory is [MANIFEST.md](./MANIFEST.md).

## Build Targets

1. `styio_cli_contract_core` builds `StyioCLI` and `StyioConfig` service code used by the full `styio` binary.
2. `styio_ide_core` builds the embeddable IDE service library.
3. `styio_lspd` builds the standalone LSP daemon.

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

