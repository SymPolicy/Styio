# CLI / Nano Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of the `styio` CLI, diagnostics surface, `styio-nano` profile pruning, and nano package bootstrap contracts.

**Last updated:** 2026-04-17

## Mission

Own user-facing command execution and the bootstrap packaging path for `styio-nano`. This team protects CLI options, error formatting, exit codes, machine-info capabilities, nano profile compile definitions, and the static nano package contract. It does not own long-term package-manager UX, which belongs to `styio-spio`.

## Owned Surface

Primary paths:

1. `src/main.cpp`
2. `src/StyioConfig/`
3. `configs/`
4. `scripts/gen-styio-nano-profile.py`
5. Nano package tests in `tests/styio_test.cpp`

Key handoff document:

1. [../for_spio/Styio-Nano-Spio-Coordination.md](../for_spio/Styio-Nano-Spio-Coordination.md)

## Daily Workflow

1. Determine whether the change affects full `styio`, `styio-nano`, or both.
2. Keep CLI option changes discoverable through help text and tests.
3. Keep `--machine-info` capability output aligned with actual behavior.
4. Treat nano static repository layout as a contract; update handoff docs when it changes.
5. Keep package-manager responsibilities out of the compiler unless they are bootstrap validation.
6. When compile-plan or diagnostics behavior changes, keep the `styio-spio` / `styio-view` coordinator mirror and handoff docs aligned in the same checkpoint.

## Change Classes

1. Small: help text, config parsing cleanup, or non-contract local path fix. Run targeted CLI or nano tests.
2. Medium: CLI option, diagnostic format, exit code, nano profile, or machine-info capability change. Update tests and docs.
3. High: nano package layout, publish/consume validation, or compiler/package-manager responsibility split. Use checkpoint workflow and review the `styio-spio` handoff.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build -R '^StyioDiagnostics\.'
ctest --test-dir build -R 'Nano|nano'
ctest --test-dir build -L milestone
```

When package behavior changes:

```bash
cmake --build build --target styio styio_nano
ctest --test-dir build -L styio_pipeline
python3 scripts/ecosystem-cli-doc-gate.py
python3 scripts/docs-audit.py
```

## Cross-Team Dependencies

1. Codegen / Runtime must review runtime capability, extern, or execution behavior surfaced through CLI.
2. Test Quality must review new CLI/nano tests and package workflow regression coverage.
3. Docs / Ecosystem must review [../for_spio/Styio-Nano-Spio-Coordination.md](../for_spio/Styio-Nano-Spio-Coordination.md) changes.
4. Frontend or Sema / IR must review CLI switches that select parser or compiler-stage behavior.

## Handoff / Recovery

Record unfinished CLI/nano work with:

1. Option or config key touched.
2. Full vs nano behavior difference.
3. Package layout or registry contract delta.
4. Exact create/publish/consume command used.
5. Whether `styio-spio` is expected to take over the responsibility later.
