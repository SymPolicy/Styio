# Styio / Pafio Handoff

**Purpose:** Freeze the narrow compiler boundary between the system-provided Styio compiler and Pafio project workflows.

**Last updated:** 2026-07-30

## Ownership

Styio owns compilation and the compiler-produced contracts:

- `styio --machine-info=json`
- `styio --compile-plan <path>`
- diagnostics
- receipts
- runtime events

Pafio owns project manifests, lock and resolution state, metadata, dependency
sync, target selection, and the `check` / `build` / `run` / `test` workflow
envelope. Pafio does not install, update, switch, pin, build, or cache Styio.
Users provide Styio through their system distribution channel.

## Compiler discovery

Pafio resolves the external compiler in this order:

1. its explicit `--styio-bin` argument;
2. `PAFIO_STYIO_BIN`;
3. `styio` on `PATH`.

The selected binary must pass the public machine-info handshake. Repository
layout, private compiler state, and source checkout paths are not part of the
handoff.

## Compile-plan v1

Pafio produces compile-plan v1 with:

```json
{
  "plan_version": 1,
  "generated_by": {
    "tool": "pafio",
    "version": "0.1.0"
  },
  "intent": "build"
}
```

The full plan also carries the selected entry, packages, resolution order,
toolchain language settings, build profile, output directories, and requested
emissions. `profile` is limited to `name`, `opt_level`, `debug`, and `lto`.
Styio rejects unsupported profile fields and producer identities.

Styio consumes the plan for all four intents:

- `check`
- `build`
- `run`
- `test`

Invalid plans and CLI conflicts remain machine-readable. When the plan exposes
an absolute `outputs.diag_dir`, Styio also writes the corresponding diagnostic
there.

## Compiler-owned results

Styio owns the concrete shape and evolution of:

- `diagnostics.jsonl`
- `receipt.json`
- `runtime-events.jsonl`
- compiler artifacts under the requested artifact directory

Pafio may surface paths and workflow status, but must not redefine compiler
diagnostics, receipt contents, or runtime-event semantics. Vityo consumes
language-service and compiler contracts directly from Styio.

## Non-goals

This handoff does not make Styio a Pafio-managed toolchain. It does not define
compiler installation, source-build orchestration, compiler channels, project
compiler pins, hosted execution, registry services, or IDE aggregation.
