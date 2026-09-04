# CLI / Nano Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of the `styio` CLI, diagnostics surface, `styio-nano` profile pruning, and nano package bootstrap contracts.

**Last updated:** 2026-09-04

## Mission

Own user-facing command execution, the bootstrap packaging path for `styio-nano`, and the compiler-side handoff contracts consumed by `pafio-nightly`. This team protects CLI options, error formatting, exit codes, machine-info capabilities, source-build metadata, nano profile compile definitions, and the static nano package contract. It does not own long-term package-manager UX, which belongs to `pafio-nightly`.

Native builds use compiler-owned content-addressed object caches for runtime and generated user objects. Cache validation, corruption recovery, concurrent publication, and privacy-safe opt-in phase profiling remain CLI correctness contracts.

## Owned Surface

Primary paths:

1. `src/main.cpp`
2. `src/StyioConfig/`
3. `configs/`
4. `scripts/gen-styio-nano-profile.py`
5. `scripts/source-build-minimal.sh`
6. Nano package tests in `tests/styio_test.cpp`

Key implementation seams inside `src/StyioConfig/`:

1. `CompilePlanContract.*` owns compile-plan version/build-mode parsing and validation shared by full `styio` execution paths.
2. `SourceBuildInfo.*` owns the published `--source-build-info=json` payload for compiler maintainers and system package builders.

Portability rule for `src/main.cpp` helpers: compare `std::filesystem::path` values by component (for example `*relative.begin() != ".."`) instead of `path::native()` string operations, because `native()` is `std::wstring` on Windows and rejects `std::string`/`const char*` operations that compile on POSIX. The windows-smoke CI job enforces this surface.

Key handoff document:

1. [../external/for-pafio/Styio-Nano-Pafio-Coordination.md](../external/for-pafio/Styio-Nano-Pafio-Coordination.md)
2. [../external/for-pafio/Styio-Ecosystem-Machine-Contract-Matrix.md](../external/for-pafio/Styio-Ecosystem-Machine-Contract-Matrix.md)

## Daily Workflow

1. Determine whether the change affects full `styio`, `styio-nano`, or both.
2. Keep CLI option changes discoverable through help text and tests.
3. Keep `--machine-info` capability output aligned with actual behavior.
4. Keep `--source-build-info=json` aligned with the official compiler source-layout contract; Pafio is not a consumer.
5. Treat nano static repository layout as a contract; update handoff docs when it changes.
6. Keep package-manager responsibilities out of the compiler unless they are bootstrap validation or official source-build layout export.
7. When compile-plan, source-build-info, or diagnostics behavior changes, keep the `pafio-nightly` / `vityo-nightly` coordinator mirror and handoff docs aligned in the same checkpoint.
8. Treat `docs/external/for-pafio/Styio-Ecosystem-Machine-Contract-Matrix.md` as the formal machine-contract handoff path; do not retain the superseded plan-tree path as a trigger or compatibility alias.
8. When runtime event artifacts change, keep `supported_contracts.runtime_events`, `feature_flags.runtime_event_stream`, `receipt.json`, and `build_root/runtime-events.jsonl` aligned in the same checkpoint.
9. Keep generated nano subset build manifests aligned with the Styio repository compatibility floor when `src/main.cpp` emits CMake scaffolding.
10. Keep `scripts/source-build-minimal.sh` aligned with the published `--source-build-info=json` contract so build-channel consumers have one stable compiler-side helper entry.
11. Prefer named enum tables and shared field-resolution helpers for project config, nano package config, nano publish config, and nano manifest parsing so new keys or aliases are added in one place instead of another `if/else` ladder in `src/main.cpp`.
12. Treat config alias changes as contract changes when they affect source-build, nano packaging, or publish bootstrap behavior; update this runbook and the handoff docs in the same checkpoint.
13. Keep compile-plan contract parsing and source-build metadata export in `src/StyioConfig/` as the single source of truth; `src/main.cpp` may orchestrate those paths, but it should not grow a second parser or duplicate build-mode vocabulary.
14. When frontend, StyioIR optimizer, or runtime source roots gain new support libraries, update the local-subset nano closure seed list, generated CMake include paths, generated config headers, and link libraries together; `StyioNanoPackage.LocalSubset*` tests must prove the extracted clean-room bundle still links.
15. When compiler source-layout directories move, update `SourceBuildInfo.*`, `styio_nano_source_roots_latest(...)`, and the `StyioDiagnostics.SourceBuildInfoJsonReportsOfficialSourceLayoutFields` regression together so system package builders see the same controlled component graph as local nano bundles.
16. When internal prelude source files such as `src/StyioPrelude/resources.styio` become part of compiler behavior, include them in `--source-build-info=json` controlled components and the matching diagnostics regression.
17. When `--profile-frontend` grows runtime-side records, keep the CLI flush hook in `src/main.cpp` paired with a profiler smoke that proves the emitted JSON includes the new section. Native executable profiling stays opt-in through `STYIO_NATIVE_PROFILE_OUT` so benchmark validation can collect run-only attribution without adding overhead to measured repeats.
18. Keep `--nano-create` clean-room local-subset builds on the same Clang CMake compiler pair used to build Styio unless `CC` or `CXX` is explicitly set by the caller; generated `build-styio-nano.sh` must preserve that override rule.
19. When Sema / IR gains a new required implementation directory such as `src/StyioResourceTopology/`, add its `.cpp` seed to `styio_nano_source_roots_latest(...)` so local-subset nano packages link in a clean-room bundle.
20. Keep `styio build <file_path> -o <artifact_name>` aligned with the native executable artifact contract: it must not execute the entry program during build, must reuse the compile-plan `intent=build` frontend path, and must link the Styio runtime helper surface into the produced executable.
21. Remove unused CLI debug helpers instead of leaving ad hoc public symbols or stdout probes in `src/main.cpp`; command-visible diagnostics should go through the existing CLI error and option paths.
22. Keep clean-room nano package builds resource-bounded by default. `STYIO_NANO_BUILD_JOBS` may raise the build parallelism on larger machines, but generated helpers should not default to unbounded `--parallel`.
23. Keep the Pafio handoff doc pointed at current contracts only. Do not reintroduce deleted bootstrap/source-build long plans after their durable rules have moved into this runbook, the repository map, or the handoff document.
24. Keep generated nano CMake portable across upstream-supported hosts: preserve Windows `file://` drive handling, do not emit `.exe` inside CMake `OUTPUT_NAME`, derive LLVM dependency roots from `LLVM_DIR`, and guard MSVC size flags so Debug builds remain debuggable.
25. When generated nano CMake needs Windows toolchain discovery paths, derive them from CMake inputs or process environment variables instead of emitting machine-specific absolute roots in `src/main.cpp`.
26. Compile-plan validation diagnostics must keep service-facing field names stable. Relative `entry.file` reports `file`, relative `outputs.artifact_dir` reports `artifact_dir`, and `outputs.diag_dir` keeps the fully qualified field because invalid diagnostics sinks cannot be written safely.
27. Windows `styio build` native executable linking must keep LLVM 18 viable with newer MSVC STL headers by emitting the established compiler/STL compatibility define and CRT warning define through the generated Clang command. Do not solve this in CTest with a machine-specific Visual Studio path.
28. Treat Pafio as the external compile-plan producer. Keep `generated_by.tool` / `generated_by.version` validation, machine-info capability advertisement, CLI contract tests, and the Pafio handoff document aligned; unsupported producer identities must fail closed instead of entering a compatibility path.
29. Callable interface publication is explicit compiler orchestration: `--emit-module-interface` requires `--file` and a canonical `--module-id`, writes the requested `.styioi` only after Sema succeeds, and is outside the compile-plan v1 envelope. Imported dependency interfaces are never synthesized as a side effect. Keep the compiler ABI digest aligned across full/nano channel, edition, target triple, and pointer width, and keep nano compile-time boundaries from linking full-only compile-plan services.
30. Configure callable-specialization identity from the same full/nano compiler ABI contract used by callable interfaces, plus the selected dictionary implementation and validated entry-dependency digest. Both binaries must produce deterministic full-digest mono symbols for the same environment; a CLI call-order change must not alter identity, and no source option may expose explicit instantiation.
31. Keep the explicit nano package source roots aligned with every translation unit in `STYIO_FRONTEND_SOURCES`, including neutral frontend support such as `StyioUtil/SemanticIdentity.cpp`, callable interface loading, and specialization graph owners. Header-closure discovery cannot compensate for an omitted `.cpp`; both local-subset creation paths must build and link the materialized `styio_nano` bundle.
32. Keep full and nano callable-interface compiler ABI identities synchronized with the active `.styioi` schema. Canonical effect rows, usage requirements, contract/body digests, and portable StyioIR schema-v1 payloads in interface schema v4 must use the `styio.callable-interface.v4` namespace in both binaries, reject schema v3 metadata before installing module facts, and bump dependent specialization/dependency fingerprint namespaces rather than retaining a dual-reader compatibility path. Keep the pure `StyioIR/PortableCallableBody.cpp` serializer/verifier support and the AST-owned `StyioLowering/PortableCallableBody.cpp` conversion support in both explicit source manifests.
33. Callable specialization disk reuse is an explicit full/nano operational option. `--callable-cache-dir` is the sole enablement switch; age, byte, and file limits without it are CLI errors. Keep default limits at seven days, 256 MiB, and 4,096 artifacts unless the feature SSOT changes, and keep limit values positive and bounded. `--callable-cache-stats` must emit exactly one path-free schema-v1 JSON object only when requested. Add every cache translation unit to the shared backend source list so full and nano use the same object schema, while their channel facts retain separate namespaces.
34. Keep the ecosystem machine-contract matrix under `docs/external/for-pafio/` as a formal contract projection, not an implementation plan. When a public command, owner, consumer, producer identity, or hosted boundary changes, update the matrix, Pafio handoff, ecosystem document gate, and consumer mirrors in one closure.
35. Run `python3 scripts/monolith-line-ratchet-gate.py` for changes that can affect `src/main.cpp`; the measured 7,584-line ceiling may decrease after extraction work but must not increase to accommodate new non-CLI responsibilities.
36. Treat callable value-ABI revisions as one-way compiler migrations. A new runtime value representation must bump the callable backend ABI marker in full and nano builds; reject older artifacts instead of dual-reading them while preserving source-language diagnostics and opt-in cache controls.

## Change Classes

1. Small: help text, config parsing cleanup, or non-contract local path fix. Run targeted CLI or nano tests.
2. Medium: CLI option, diagnostic format, exit code, nano profile, machine-info capability, or runtime event artifact change. Update tests and docs.
3. High: nano package layout, publish/consume validation, compiler/package-manager responsibility split, or config parser alias/table changes that affect bootstrap contracts. Use checkpoint workflow and review the `pafio-nightly` handoff.

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
3. Docs / Ecosystem must review [../external/for-pafio/Styio-Nano-Pafio-Coordination.md](../external/for-pafio/Styio-Nano-Pafio-Coordination.md) changes.
4. Frontend or Sema / IR must review CLI switches that select parser or compiler-stage behavior.

## Handoff / Recovery

Record unfinished CLI/nano work with:

1. Option or config key touched.
2. Full vs nano behavior difference.
3. Package layout or registry contract delta.
4. Exact create/publish/consume command used.
5. Whether `pafio-nightly` is expected to take over the responsibility later.
