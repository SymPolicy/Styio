# StyioConfig

**Purpose:** Provide machine-readable compiler handoff contracts and shared configuration helpers for external tools.

**Last updated:** 2026-05-14

## Use

`StyioConfig` backs these public CLI contracts:

```bash
styio --machine-info=json
styio --source-build-info=json
styio --compile-plan path/to/compile-plan.json
```

It also provides reusable C++ helpers:

```cpp
#include "StyioServices/StyioConfig/CompilePlanContract.hpp"
#include "StyioServices/StyioConfig/SourceBuildInfo.hpp"

styio::config::CompilePlanRequest request;
std::string error;
bool ok = styio::config::parse_compile_plan("compile-plan.json", request, error);
```

## Available Functions

| Function or Type | Header | Use |
|------------------|--------|-----|
| `CompilePlanRequest` | `CompilePlanContract.hpp` | Holds the normalized request envelope consumed by compiler build/check/run/test flows. |
| `probe_compile_plan_diag_dir(...)` | `CompilePlanContract.hpp` | Extracts a diagnostics directory before full plan validation. |
| `parse_compile_plan(...)` | `CompilePlanContract.hpp` | Parses and validates the resolved compile-plan JSON contract. |
| `SourceBuildInfoOptions` | `SourceBuildInfo.hpp` | Carries compiler version, channel, and edition metadata for source-build info output. |
| `default_source_origin()` | `SourceBuildInfo.hpp` | Returns the official source origin advertised to source-build consumers. |
| `source_branch_for_channel(...)` | `SourceBuildInfo.hpp` | Maps binary channel names to source branches. |
| `source_build_info_json(...)` | `SourceBuildInfo.hpp` | Emits the JSON source-build contract. |
| `NanoProfile.hpp` macros | `NanoProfile.hpp` | Publish full/nano compile-time feature flags used by shared compiler and runtime code. |

## Contract Notes

1. `--machine-info=json` is the binary capability handshake.
2. `--source-build-info=json` is the official source-layout handshake.
3. `--compile-plan` is the request envelope for compiler execution workflows.
4. Source-build controlled components include `compiler_core`, `std_symbols`, `runtime`, `services`, and `macro_prelude`.

See the full service inventory in [../MANIFEST.md](../MANIFEST.md).
