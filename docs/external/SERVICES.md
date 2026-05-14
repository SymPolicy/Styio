# Styio Services

**Purpose:** Define the public service layer exposed by `styio` for external tools, package managers, editors, IDEs, and validation pipelines.

**Last updated:** 2026-05-14

## Scope

`StyioServices` is the repository home for language-facing services that can be consumed outside the core compiler implementation. These services are not special-case adapters for `spio`, Vityo, or any single internal application. They are the reusable frontend surface for tools that need to inspect, validate, build, or provide editor support for Styio source code.

Primary source roots:

1. `src/StyioServices/StyioCLI/` - command-line service entry helpers such as syntax checking.
2. `src/StyioServices/StyioConfig/` - compiler-side machine-readable handoff contracts.
3. `src/StyioServices/StyioIDE/` - in-process IDE services, syntax snapshots, HIR, SemDB, VFS, and completion/hover support.
4. `src/StyioServices/StyioLSP/` - stdio LSP daemon surface built on `StyioIDE`.

The build target names remain stable: `styio_cli_contract_core`, `styio_ide_core`, `styio`, and `styio_lspd`.

The source-level service entrypoint is [../../src/StyioServices/README.md](../../src/StyioServices/README.md), and the complete capability manifest is [../../src/StyioServices/MANIFEST.md](../../src/StyioServices/MANIFEST.md).

## Current Service Surfaces

### Syntax Check CLI

Canonical invocation:

```text
styio check --syntax --json --file <path>
```

This service runs lexing, parsing, and AST construction only. It does not type-check, lower to Styio IR, codegen, execute, or access runtime resources such as `@stdin`, `@file`, `@stdout`, `||>`, or `?|`.

Successful output uses the `syntax-check` contract:

```json
{"schema_version":1,"contract":"syntax-check","status":"ok","ok":true,"phase":"parse","diagnostics":[]}
```

Failure output remains JSON and carries a phase-specific diagnostic:

```json
{"schema_version":1,"contract":"syntax-check","status":"syntax_error","ok":false,"phase":"parse","diagnostics":[{"phase":"parse","code":"STYIO_PARSE","severity":"error","line":1,"column":1}]}
```

### Compiler Handoff Contracts

These contracts are still command-line surfaces, but their implementation now lives under `src/StyioServices/StyioConfig/`:

1. `styio --machine-info=json`
2. `styio --compile-plan <path>`
3. `styio --source-build-info=json`

They are suitable for package managers, build orchestrators, CI systems, and other tooling that needs stable compiler capability discovery or build/check/run/test handoff.

### IDE C++ Services

`src/StyioServices/StyioIDE/` exposes the in-process API for hosts that embed Styio support directly:

1. `styio::ide::IdeService`
2. `styio::ide::SyntaxParser`
3. `styio::ide::SemanticDB`
4. `styio::ide::HirModule`
5. `styio::ide::analyze_document`

See [for-ide/CXX-API.md](./for-ide/CXX-API.md).

### LSP

`styio_lspd` remains the editor-neutral LSP entrypoint. It is built from `src/StyioServices/StyioLSP/` and can be consumed by any IDE or editor that speaks LSP over stdio.

See [for-ide/LSP.md](./for-ide/LSP.md).

## Contract Rule

New externally consumable language services should land under `src/StyioServices/` first, then expose one of:

1. a CLI contract,
2. an in-process C++ API,
3. an LSP protocol surface,
4. or a future stable FFI/WASM boundary.

Do not add consumer-specific parser copies or editor-specific grammars when the service can reuse the compiler-owned lexer/parser or the `StyioIDE` service layer.
