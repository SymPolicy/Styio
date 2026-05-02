# CLI / Nano Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of the `styio` CLI, diagnostics surface, `styio-nano` profile pruning, and nano package bootstrap contracts.

**Last updated:** 2026-04-30

## Mission

Own user-facing command execution, the bootstrap packaging path for `styio-nano`, and the compiler-side handoff contracts consumed by `styio-spio`. This team protects CLI options, error formatting, exit codes, machine-info capabilities, source-build metadata, nano profile compile definitions, and the static nano package contract. It does not own long-term package-manager UX, which belongs to `styio-spio`.

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
2. `SourceBuildInfo.*` owns the published `--source-build-info=json` payload and its mapping to the official `spio build` source-layout contract.

Key handoff document:

1. [../external/for-spio/Styio-Nano-Spio-Coordination.md](../external/for-spio/Styio-Nano-Spio-Coordination.md)

## Daily Workflow

1. Determine whether the change affects full `styio`, `styio-nano`, or both.
2. Keep CLI option changes discoverable through help text and tests.
3. Keep `--machine-info` capability output aligned with actual behavior.
4. Keep `--source-build-info=json` aligned with the official source-layout contract consumed by `spio build`.
5. Treat nano static repository layout as a contract; update handoff docs when it changes.
6. Keep package-manager responsibilities out of the compiler unless they are bootstrap validation or official source-build layout export.
7. When compile-plan, source-build-info, or diagnostics behavior changes, keep the `styio-spio` / `styio-view` coordinator mirror and handoff docs aligned in the same checkpoint.
8. When runtime event artifacts change, keep `supported_contracts.runtime_events`, `feature_flags.runtime_event_stream`, `receipt.json`, and `build_root/runtime-events.jsonl` aligned in the same checkpoint.
9. Keep generated nano subset build manifests aligned with the repository compatibility floor and the shared `styio-nightly` / `styio-spio` toolchain baseline when `src/main.cpp` emits CMake scaffolding.
10. Keep `scripts/source-build-minimal.sh` aligned with the published `--source-build-info=json` contract so build-channel consumers have one stable compiler-side helper entry.
11. Prefer named enum tables and shared field-resolution helpers for project config, nano package config, nano publish config, and nano manifest parsing so new keys or aliases are added in one place instead of another `if/else` ladder in `src/main.cpp`.
12. Treat config alias changes as contract changes when they affect source-build, nano packaging, or publish bootstrap behavior; update this runbook and the handoff docs in the same checkpoint.
13. Keep compile-plan contract parsing and source-build metadata export in `src/StyioConfig/` as the single source of truth; `src/main.cpp` may orchestrate those paths, but it should not grow a second parser or duplicate build-mode vocabulary.
14. When frontend or runtime source roots gain new support libraries, update the local-subset nano closure seed list, generated CMake include paths, generated config headers, and link libraries together; `StyioNanoPackage.LocalSubset*` tests must prove the extracted clean-room bundle still links.
15. When compiler source-layout directories move, update `SourceBuildInfo.*`, `styio_nano_source_roots_latest(...)`, and the `StyioDiagnostics.SourceBuildInfoJsonReportsOfficialSourceLayoutFields` regression together so `spio build` consumers see the same controlled component graph as local nano bundles.

## Change Classes

1. Small: help text, config parsing cleanup, or non-contract local path fix. Run targeted CLI or nano tests.
2. Medium: CLI option, diagnostic format, exit code, nano profile, machine-info capability, or runtime event artifact change. Update tests and docs.
3. High: nano package layout, publish/consume validation, compiler/package-manager responsibility split, or config parser alias/table changes that affect bootstrap contracts. Use checkpoint workflow and review the `styio-spio` handoff.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build/default -R '^StyioDiagnostics\.'
ctest --test-dir build/default -R 'Nano|nano'
ctest --test-dir build/default -L milestone
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
