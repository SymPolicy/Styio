# Styio Services

**Purpose:** Define the public service layer exposed by `styio` for external tools, package managers, editors, IDEs, and validation pipelines.

**Last updated:** 2026-05-30

## Scope

`StyioServices` is the repository home for language-facing services that can be consumed outside the core compiler implementation. These services are the reusable frontend surface for tools that need to inspect, validate, build, or provide editor support for Styio source code.

The service layer is consumer-neutral, but first-party projects such as Vityo and `spio` may bind to it more deeply than a generic external editor. Those convenience adapters must still reuse the shared StyioServices facts, capability states, diagnostic taxonomy, parser evidence, and workspace identity instead of creating separate grammar, diagnostic, or semantic authorities.

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

This service runs lexing, authoritative nightly parsing, and AST construction only. It does not type-check, lower to Styio IR, codegen, execute, or access runtime resources such as `@stdin`, `@file`, `@stdout`, `||>`, or `?|`.

Successful output uses the `syntax-check` contract:

```json
{"schema_version":1,"contract":"syntax-check","status":"ok","ok":true,"phase":"parse","diagnostics":[]}
```

Failure output remains JSON and carries phase-specific diagnostics. Parser failures may continue after a failed statement to collect multiple `STYIO_PARSE_UNEXPECTED_TOKEN` or `STYIO_PARSE_UNSUPPORTED_SYNTAX` diagnostics, but any parser diagnostic keeps the result at `status:"syntax_error"`. Each diagnostic includes a structured `source_context` object and a `notes` array for Clang-style rendering without parsing the human message:

```json
{"schema_version":1,"contract":"syntax-check","status":"syntax_error","ok":false,"phase":"parse","diagnostics":[{"phase":"parse","code":"STYIO_PARSE_UNEXPECTED_TOKEN","severity":"error","line":1,"column":1,"offset":0,"length":1,"source_context":{"line_text":"# broken := (a: i32) => a +","range_start_column":1,"range_end_column":2,"caret":"^"},"notes":[]}]}
```

### Compiler Handoff Contracts

These contracts are still command-line surfaces, but their implementation now lives under `src/StyioServices/StyioConfig/`:

1. `styio --machine-info=json`
2. `styio --compile-plan <path>`
3. `styio --source-build-info=json`

They are suitable for package managers, build orchestrators, CI systems, and other tooling that needs stable compiler capability discovery or build/check/run/test handoff.

Compile-plan and runtime JSONL diagnostics use the same public diagnostic identity fields: `schema_version`, `contract:"diagnostic"`, `severity`, `phase`, `category`, `code`, optional compatibility `subcode`, `file`, coarse source span fields, `message`, and `notes`. Feature-owned compiler diagnostics may use narrower codes such as `STYIO_SEMA_IMMUTABLE_BINDING`, `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`, `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`, or `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE` instead of broad fallback families when the condition has focused tests and a stable owner.

### IDE C++ Services

`src/StyioServices/StyioIDE/` exposes the in-process API for hosts that embed Styio support directly:

1. `styio::ide::IdeService`
2. `styio::ide::SyntaxParser`
3. `styio::ide::SemanticDB`
4. `styio::ide::HirModule`
5. `styio::ide::analyze_document`

See [for-ide/CXX-API.md](./for-ide/CXX-API.md).

IDE syntax snapshots are editor interaction data, not a second accepted grammar. Hosts that need syntax validity must use the authoritative syntax-check contract or another compiler-owned parser service.

IDE diagnostics expose `Diagnostic::code` and `Diagnostic::phase`. Compiler-owned diagnostics use `source:"styio-compiler"` and shared compiler/service codes. Editor-only snapshot diagnostics use `source:"styio-editor"` and service codes.

### LSP

`styio_lspd` remains the editor-neutral LSP entrypoint. It is built from `src/StyioServices/StyioLSP/` and can be consumed by any IDE or editor that speaks LSP over stdio.

LSP `publishDiagnostics` maps Styio identity into `Diagnostic.code` and `Diagnostic.data.phase`.

See [for-ide/LSP.md](./for-ide/LSP.md).

## Contract Rule

New externally consumable language services should land under `src/StyioServices/` first, then expose one of:

1. a CLI contract,
2. an in-process C++ API,
3. an LSP protocol surface,
4. or a future stable FFI/WASM boundary.

Do not add consumer-specific parser copies or editor-specific accepted grammars. The hand-written nightly compiler parser is the grammar authority for all external services.

First-party adapters may expose richer payloads than public LSP when they are documented as StyioServices facts and marked with capability state. Public LSP capabilities remain editor-neutral and may be advertised only after the backing `StyioIDE` behavior and tests exist.
