# StyioConfig

**Purpose:** Provide machine-readable compiler handoff contracts and shared configuration helpers for external tools.

**Last updated:** 2026-09-05

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
| `CompilationUnit` | `CompilePlanContract.hpp` | Holds the admitted package name plus canonical manifest-relative and entry-relative paths used for qualified snapshot identity. Absolute paths never enter this value. |
| `CompilePlanRequest::emit_observable_static_snapshot` | `CompilePlanContract.hpp` | Optional incubating request for schema-v1 static snapshot publication. Absent means the compiler must not construct snapshot state. |
| `CompilePlanRequest::observable_static_snapshot_parent_snapshot_path` | `CompilePlanContract.hpp` | Optional `emit.observable_static_snapshot.parent_snapshot_path` (non-empty string, otherwise `observable_static_snapshot_malformed`). Transport input for producer-owned delta emission only; never part of admission, identity, or snapshot bytes. Empty when absent. |
| `compile_plan_artifact_stem(...)` | `CompilePlanContract.hpp` | Derives the `<stem>` shared by every compile-plan artifact under `artifact_dir` from the entry target name or entry file. Header-inline so nano compiles without the contract library. |
| `SourceBuildInfoOptions` | `SourceBuildInfo.hpp` | Carries compiler version, channel, and edition metadata for source-build info output. |
| `default_source_origin()` | `SourceBuildInfo.hpp` | Returns the official source origin advertised to source-build consumers. |
| `source_branch_for_channel(...)` | `SourceBuildInfo.hpp` | Maps binary channel names to source branches. |
| `source_build_info_json(...)` | `SourceBuildInfo.hpp` | Emits the JSON source-build contract. |
| `NanoProfile.hpp` macros | `NanoProfile.hpp` | Publish full/nano compile-time feature flags used by shared compiler and runtime code. |

## Contract Notes

1. `--machine-info=json` is the binary capability handshake.
2. `--source-build-info=json` is the official source-layout handshake.
3. `--compile-plan` is the request envelope for compiler execution workflows.
4. Optional `emit.observable_static_snapshot` is an incubating, unapproved, static-only request. It is admitted only for one qualified Pafio package entry, writes `<stem>.observable-static-snapshot.json`, and is listed on the compile receipt. Its optional `parent_snapshot_path` additionally requests `<stem>.observable-delta.json` and a receipt `observable_static_snapshot` record whose degradation never fails the compile. The schema, capability names, receipt shapes, and decoder contract live in [../StyioObservable/README.md](../StyioObservable/README.md).
5. Source-build controlled components include `compiler_core`, `std_symbols`, `runtime`, `services`, and `macro_prelude`.

See the full service inventory in [../MANIFEST.md](../MANIFEST.md).
