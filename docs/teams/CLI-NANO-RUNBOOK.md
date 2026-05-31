# CLI / Nano Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of the `styio` CLI, diagnostics surface, `styio-nano` profile pruning, and nano package bootstrap contracts.

**Last updated:** 2026-05-30

## Mission

Own user-facing command execution, the bootstrap packaging path for `styio-nano`, and compiler-side service contracts consumed by package managers, IDEs, CI, and other external tools. This team protects CLI options, error formatting, exit codes, machine-info capabilities, syntax-check output, source-build metadata, nano profile compile definitions, and the static nano package contract. It does not own long-term package-manager UX, which belongs to `styio-spio`.

## Owned Surface

Primary paths:

1. `src/main.cpp`
2. `src/StyioServices/StyioCLI/`
3. `src/StyioServices/StyioConfig/`
4. `configs/`
5. `scripts/gen-styio-nano-profile.py`
6. `scripts/source-build-minimal.sh`
7. Nano package tests in `tests/styio_test.cpp`

Key implementation seams inside `src/StyioServices/`:

1. `StyioCLI/SyntaxCheck.*` owns `styio check --syntax --json --file`, the syntax-only JSON service for IDEs, CI, and other tools.
2. `StyioConfig/CompilePlanContract.*` owns compile-plan version/build-mode parsing and validation shared by full `styio` execution paths.
3. `StyioConfig/SourceBuildInfo.*` owns the published `--source-build-info=json` payload and its mapping to the official `spio build` source-layout contract.

Key handoff document:

1. [../external/for-spio/Styio-Nano-Spio-Coordination.md](../external/for-spio/Styio-Nano-Spio-Coordination.md)

## Daily Workflow

1. Determine whether the change affects full `styio`, `styio-nano`, or both.
2. Keep CLI option changes discoverable through help text and tests.
3. Keep `--machine-info` capability output aligned with actual behavior.
4. Keep `styio check --syntax --json --file` syntax-only: no type checking, lowering, codegen, execution, or runtime resource access.
5. Keep `--source-build-info=json` aligned with the official source-layout contract consumed by `spio build`.
6. Treat nano static repository layout as a contract; update handoff docs when it changes.
7. Keep package-manager responsibilities out of the compiler unless they are bootstrap validation or official source-build layout export.
8. When compile-plan, source-build-info, syntax-check, or diagnostics behavior changes, keep the coordinator mirror, external service docs, source-level module README, and `src/StyioServices/MANIFEST.md` aligned in the same checkpoint.
9. When runtime event artifacts change, keep `supported_contracts.runtime_events`, `feature_flags.runtime_event_stream`, `receipt.json`, and `build_root/runtime-events.jsonl` aligned in the same checkpoint.
10. Keep generated nano subset build manifests aligned with the repository compatibility floor and the shared `styio-nightly` / `styio-spio` toolchain baseline when `src/main.cpp` emits CMake scaffolding.
11. Keep `scripts/source-build-minimal.sh` aligned with the published `--source-build-info=json` contract so build-channel consumers have one stable compiler-side helper entry.
12. Prefer named enum tables and shared field-resolution helpers for project config, nano package config, nano publish config, and nano manifest parsing so new keys or aliases are added in one place instead of another `if/else` ladder in `src/main.cpp`.
13. Treat config alias changes as contract changes when they affect source-build, nano packaging, or publish bootstrap behavior; update this runbook and the handoff docs in the same checkpoint.
14. Keep compile-plan contract parsing and source-build metadata export in `src/StyioServices/StyioConfig/` as the single source of truth; `src/main.cpp` may orchestrate those paths, but it should not grow a second parser or duplicate build-mode vocabulary.
15. When frontend, StyioIR optimizer, or runtime source roots gain new support libraries, update the local-subset nano closure seed list, generated CMake include paths, generated config headers, and link libraries together; `StyioNanoPackage.LocalSubset*` tests must prove the extracted clean-room bundle still links.
16. When compiler source-layout directories move, update `SourceBuildInfo.*`, `styio_nano_source_roots_latest(...)`, and the `StyioDiagnostics.SourceBuildInfoJsonReportsOfficialSourceLayoutFields` regression together so `spio build` consumers see the same controlled component graph as local nano bundles.
17. When internal prelude source files such as `src/StyioPrelude/resources.styio` become part of compiler behavior, include them in `--source-build-info=json` controlled components and the matching diagnostics regression.
18. When `--profile-frontend` grows runtime-side records, keep the CLI flush hook in `src/main.cpp` paired with a profiler smoke that proves the emitted JSON includes the new section. Native executable profiling stays opt-in through `STYIO_NATIVE_PROFILE_OUT` so benchmark validation can collect run-only attribution without adding overhead to measured repeats.
19. Keep `--nano-create` clean-room local-subset builds on the same Clang CMake compiler pair used to build Styio unless `CC` or `CXX` is explicitly set by the caller; generated `build-styio-nano.sh` must preserve that override rule.
20. When Sema / IR gains a new required implementation directory or pass such as `src/StyioResourceTopology/` or the StyioIR verifier, add its `.cpp` seed to `styio_nano_source_roots_latest(...)` so local-subset nano packages link in a clean-room bundle.
21. Keep `styio build <file_path> -o <artifact_name>` aligned with the native executable artifact contract: it must not execute the entry program during build, must reuse the compile-plan `intent=build` frontend path, and must link the Styio runtime helper surface into the produced executable.
22. Native executable artifact linking must use a clang-family C++ driver because the build path links generated LLVM `.ll` IR directly; only `STYIO_NATIVE_CXX` may intentionally override that probe, and native artifact tests must cover the system-compiler fallback.
23. Remove unused CLI debug helpers instead of leaving ad hoc public symbols or stdout probes in `src/main.cpp`; command-visible diagnostics should go through the existing CLI error and option paths.
24. Keep clean-room nano package builds resource-bounded by default. `STYIO_NANO_BUILD_JOBS` may raise the build parallelism on larger machines, but generated helpers should not default to unbounded `--parallel`.
25. Keep the Spio handoff doc pointed at current contracts only. Do not reintroduce deleted bootstrap/source-build long plans after their durable rules have moved into this runbook, the repository map, or the handoff document.
26. When `styio check --syntax --json --file` diagnostics gain recovery behavior or new machine-readable fields, update `--machine-info=json` capabilities, `StyioDiagnostics.*` contract tests, `src/StyioServices/StyioCLI/README.md`, and `src/StyioServices/MANIFEST.md` together.
27. Keep syntax-check parser authority locked to nightly. `--parser-engine nightly` may remain accepted as an explicit spelling, but `legacy`, `new`, and consumer-specific engines must remain CLI errors for the public syntax service.
28. Public JSON/JSONL diagnostics must use the shared `STYIO_<PHASE>_<ERROR_FAMILY>` taxonomy from `src/StyioServices/DiagnosticContract.hpp`; keep compatibility `subcode` fields only as secondary aliases.
29. For IM-D10 package-boundary work, keep `styio` limited to compiler-side facts: machine-info, source-build-info, syntax-check, compile-plan, nano producer/verifier, receipts, diagnostics, and runtime events. Do not infer current Spio or Styio-Platform behavior from stale local checkouts; unresolved manifest, lockfile, resolver, registry, trust, hosted workspace, standard-library package, and compatibility-matrix topics must remain external confirmation items.
30. Recovery edits under `src/StyioServices/StyioConfig/` should be mechanical unless they change a public CLI contract. Whitespace-only README cleanup does not alter machine-info, source-build-info, compile-plan, or nano behavior and should be paired with docs gates rather than CLI contract rewrites.
31. `src/main.cpp` carries embedded TOML project-config parsing, the styio-nano package/publish/manifest workflow, the `--machine-info=json` printer, and the parser shadow-compare driver in addition to CLI dispatch. This is tracked as **M-CLI-01** in [`../rollups/MIGRATION-LEDGER.md`](../rollups/MIGRATION-LEDGER.md); incremental moves should land non-CLI logic under `src/StyioServices/StyioConfig/` (or a future `src/StyioCLI/`) instead of growing `main.cpp` further.
32. Nano negative-path coverage must track the compiler-owned handoff surface, not package-manager UX. Keep malformed repository markers and entries, malformed cloud manifests, blob hash/size mismatches, remote publish roots, create/publish mutual exclusion, missing nano mode selection, and create-only/publish-only option mixups covered by `StyioNanoPackage.*` tests before broadening static repository behavior.
33. Compile-plan v1 malformed-input hardening must stay inside the resolved compiler request envelope: explicit optional fields with the wrong shape should fail instead of silently defaulting, package entries must have compiler-visible `id` fields, and `entry.package_id` must be present in the package list. Do not add resolver, install, registry, or package lifecycle behavior to close those compiler-side guards.
34. When a public JSONL diagnostic family is narrowed from a broad fallback such as `STYIO_TYPE_ERROR`, add the code and classifier in `DiagnosticContract.hpp`, keep the process exit family stable unless the contract explicitly changes, update focused CLI JSONL tests, and refresh the StyioServices docs plus IM-D3 inventory in the same checkpoint.
35. Immutable/final binding mutation diagnostics are sema-family public facts even when they surface through the TypeError exit family. Keep `STYIO_SEMA_IMMUTABLE_BINDING`, phase `sema`, exit code 4, and the stable message fragment covered together when those classifier messages change.
36. Tuple function return annotation diagnostics are type-family public facts over an existing fail-closed boundary. Keep `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`, phase `type`, exit code 4, and the stable tuple-value-IR message fragment covered without implying tuple value IR support.
37. Hash-tag iterator route diagnostics are type-family public facts over an existing fail-closed boundary. Keep `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`, phase `type`, exit code 4, the stable route-not-implemented message, and no-following-output coverage together without implying IM-D5-P1 route semantics.
38. Resource-effect/resource-method diagnostic refinements are public JSONL facts over existing fail-closed boundaries. Keep `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH` with phase `type`, exit code 4, and the stable fallback mismatch fragment, and keep `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY` with phase `sema`, exit code 4, the stable scalar-local preface message fragment, and no-following-output coverage. Do not treat either code as resource-effect semantics expansion.
39. Native interop diagnostic refinements are public JSONL facts over existing fail-closed boundaries. Keep `STYIO_NATIVE_SOURCE_READ_FAILED`, `STYIO_NATIVE_SIGNATURE_NOT_FOUND`, `STYIO_NATIVE_UNSUPPORTED_SIGNATURE`, `STYIO_NATIVE_HOST_COMPILE_FAILED`, `STYIO_NATIVE_LOAD_FAILED`, `STYIO_NATIVE_SYMBOL_MISSING`, and `STYIO_NATIVE_TOOLCHAIN_UNAVAILABLE` with phase `native_interop`, exit code 4 for the current sema/codegen TypeError route, stable message fragments, and no-following-output coverage without treating them as native ABI, signature support, artifact loading, symbol visibility, host compiler behavior, or toolchain behavior expansion.
40. Undeclared-symbol diagnostic refinements are sema-family public facts over existing fail-closed resolution boundaries. Keep `STYIO_SEMA_UNDECLARED_SYMBOL`, phase `sema`, exit code 4 for the current TypeError route, stable unknown function/resource message fragments, and no-following-output coverage without treating them as broader symbol resolution, import, resource lookup, or hidden native symbol visibility support.
41. Call-arity diagnostic refinements are sema-family public facts over existing fail-closed call resolution boundaries. Keep `STYIO_SEMA_CALL_ARITY_MISMATCH`, phase `sema`, exit code 4 for the current TypeError route, stable user function/resource-method `expects N argument(s), got M` fragments, and no-following-output coverage without treating them as broader function calling or resource-method dispatch support.
42. Call argument-type diagnostic refinements are type-family public facts over existing fail-closed call compatibility checks. Keep `STYIO_TYPE_CALL_ARGUMENT_MISMATCH`, phase `type`, exit code 4 for the current TypeError route, stable user function/resource-method argument mismatch fragments, and no-following-output coverage without treating them as broader function calling, implicit argument adaptation, native ABI behavior, or resource-method dispatch support.
43. Resource capability diagnostic refinements are sema-family public facts over existing fail-closed resource capability checks. Keep `STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH`, phase `sema`, exit code 4 for the current TypeError route, stable capability message fragments, and no-following-output coverage without treating the code as new resource-family support, pressure observers, or fallback recovery behavior.
44. Pressure observer unsupported-family diagnostics are sema-family public facts over a parser/Sema boundary. Keep `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`, phase `sema`, exit code 4 for the current TypeError route, stable unsupported-family message fragments, and no-following-output coverage without treating the code as pressure payload typing, observer execution, runtime pressure-stream support, or backpressure scheduling behavior.
45. Typed stdin unsupported-target diagnostics are type-family public facts over existing fail-closed scalar/list target checks. Keep `STYIO_TYPE_STDIN_UNSUPPORTED_TARGET`, phase `type`, exit code 4 for the current TypeError route, stable unsupported-target message fragments, and no-following-output coverage without treating the code as new stdin target support or resource-effect recovery behavior.

## Change Classes

1. Small: help text, config parsing cleanup, or non-contract local path fix. Run targeted CLI or nano tests.
2. Medium: CLI option, diagnostic format, exit code, nano profile, machine-info capability, or runtime event artifact change. Update tests and docs.
3. High: nano package layout, publish/consume validation, compiler/package-manager responsibility split, or config parser alias/table changes that affect bootstrap contracts. Use checkpoint workflow and review the `styio-spio` handoff.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build/default -R '^StyioDiagnostics\.'
ctest --test-dir build/default -R 'Nano|nano'
ctest --test-dir build/default -L language_feature
```

When package behavior changes:

```bash
cmake --build build/default --target styio styio_nano
ctest --test-dir build/default -L styio_pipeline
python3 scripts/ecosystem-cli-doc-gate.py
python3 scripts/docs-audit.py
```

## Cross-Team Dependencies

1. Codegen / Runtime must review runtime capability, extern, or execution behavior surfaced through CLI.
2. Test Quality must review new CLI/nano tests and package workflow regression coverage.
3. Docs / Ecosystem must review [../external/for-spio/Styio-Nano-Spio-Coordination.md](../external/for-spio/Styio-Nano-Spio-Coordination.md) changes.
4. Frontend or Sema / IR must review CLI switches that select parser or compiler-stage behavior.

## Handoff / Recovery

Record unfinished CLI/nano work with:

1. Option or config key touched.
2. Full vs nano behavior difference.
3. Package layout or registry contract delta.
4. Exact create/publish/consume command used.
5. Whether `styio-spio` is expected to take over the responsibility later.
