# StyioServices Manifest

**Purpose:** List every public service capability currently available under `src/StyioServices/`.

**Last updated:** 2026-05-31

## Capability Inventory

| Capability | Module | Entry Point | Current Form | Use |
|------------|--------|-------------|--------------|-----|
| Shared diagnostic taxonomy | `StyioServices` | `DiagnosticContract.hpp` | Header-only C++ contract | Keep public diagnostic phases and stable `STYIO_<PHASE>_<ERROR_FAMILY>` codes shared across CLI, compile-plan, runtime, IDE, and LSP surfaces. |
| Syntax-only JSON check | `StyioCLI` | `styio check --syntax --json --file <path>` / `styio::services::run_syntax_check_cli` | CLI contract | Validate Styio source syntax with the authoritative nightly parser, without type checking, lowering, codegen, execution, or runtime resource access. |
| Parser authority lock for syntax check | `StyioCLI` | `--parser-engine nightly` | CLI option | Preserve compatibility with explicit engine selection while rejecting non-authoritative engines such as `legacy`. |
| Syntax-check recovery diagnostics | `StyioCLI` | `styio check --syntax --json --file <path>` | CLI contract behavior | Continue parsing after recoverable statement-level syntax failures and report multiple stable-code diagnostics in one JSON result. |
| Syntax-check source context | `StyioCLI` | `diagnostics[].source_context` | JSON diagnostic field | Provide source line text, range columns, and caret marker data for IDE and terminal renderers. |
| Syntax-check diagnostic codes | `StyioCLI` | `diagnostics[].code`, `diagnostics[].phase`, `diagnostics[].notes` | JSON diagnostic fields | Expose stable lex, parse, and service diagnostic codes such as `STYIO_LEX_UNTERMINATED_BLOCK_COMMENT`, `STYIO_PARSE_UNEXPECTED_TOKEN`, and `STYIO_SERVICE_INVALID_ARGUMENT`. |
| Compiler JSONL diagnostic codes | `src/main.cpp` + `DiagnosticContract.hpp` | `styio --error-format=jsonl --file <path>` and compile-plan `diag_dir/diagnostics.jsonl` | CLI JSONL diagnostic fields | Expose stable compiler/runtime diagnostic families beyond syntax-check, including feature-owned sema/type/native codes such as `STYIO_SEMA_IMMUTABLE_BINDING`, `STYIO_SEMA_UNDECLARED_SYMBOL`, `STYIO_SEMA_CALL_ARITY_MISMATCH`, `STYIO_SEMA_RESOURCE_CAPABILITY_MISMATCH`, `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`, `STYIO_SEMA_RESOURCE_METHOD_UNSUPPORTED_BODY`, `STYIO_TYPE_RESOURCE_EFFECT_FALLBACK_MISMATCH`, `STYIO_TYPE_CALL_ARGUMENT_MISMATCH`, `STYIO_TYPE_UNSUPPORTED_TUPLE_RETURN`, `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`, `STYIO_TYPE_STREAM_ZIP_UNSUPPORTED_SOURCE`, `STYIO_TYPE_ITERATION_UNSUPPORTED_SOURCE`, `STYIO_TYPE_STDIN_UNSUPPORTED_TARGET`, `STYIO_NATIVE_SOURCE_READ_FAILED`, `STYIO_NATIVE_SIGNATURE_NOT_FOUND`, `STYIO_NATIVE_UNSUPPORTED_SIGNATURE`, `STYIO_NATIVE_HOST_COMPILE_FAILED`, `STYIO_NATIVE_LOAD_FAILED`, `STYIO_NATIVE_SYMBOL_MISSING`, and `STYIO_NATIVE_TOOLCHAIN_UNAVAILABLE`. |
| Machine info contract | `StyioConfig` via `src/main.cpp` | `styio --machine-info=json` | CLI JSON contract | Discover compiler version, release channel, supported contracts, feature flags, adapter modes, and capabilities. |
| Compile-plan contract parsing | `StyioConfig` | `styio::config::parse_compile_plan` / `styio --compile-plan <path>` | C++ helper plus CLI contract | Parse and validate versioned compiler request envelopes for build, check, run, and test handoff. |
| Compile-plan diagnostics directory probing | `StyioConfig` | `styio::config::probe_compile_plan_diag_dir` | C++ helper | Find the requested diagnostics directory early enough for machine-readable service errors such as `STYIO_SERVICE_COMPILE_PLAN_INVALID`. |
| Build-mode vocabulary | `StyioConfig` | `default_build_mode_name`, `is_supported_build_mode` | C++ helper | Keep source-build and compile-plan build-mode handling centralized. |
| Source-build info contract | `StyioConfig` | `styio::config::source_build_info_json` / `styio --source-build-info=json` | C++ helper plus CLI JSON contract | Publish official source origin, source channel mapping, public build entrypoints, and controlled source components. |
| Nano profile feature macros | `StyioConfig` | `NanoProfile.hpp` | Compile-time configuration | Publish full/nano feature flags and pruning controls to shared compiler/runtime code. |
| URI and text-position primitives | `StyioIDE` | `Common.hpp` | C++ API | Convert URI/path values, map offsets to positions, and share editor-facing data structures. |
| Virtual file system snapshots | `StyioIDE` | `styio::ide::VirtualFileSystem` | C++ API | Open, update, incrementally edit, close, and snapshot documents. |
| Editor syntax snapshots | `StyioIDE` | `styio::ide::SyntaxParser` | C++ API | Produce non-authoritative token, node, diagnostic, matching-token, and folding snapshots for editing features. |
| Compiler semantic bridge | `StyioIDE` | `styio::ide::analyze_document` | C++ API | Reuse the authoritative nightly parser/type facts for IDE semantic summaries and diagnostics without recovery parsing. |
| IDE diagnostic code bridge | `StyioIDE` | `styio::ide::Diagnostic::code` / `phase` | C++ API fields | Preserve compiler/service diagnostic identity while marking editor-only diagnostics as `styio-editor` service diagnostics. |
| HIR construction | `StyioIDE` | `styio::ide::HirBuilder` | C++ API | Build stable editor-facing item, scope, symbol, and reference models from syntax and semantic facts. |
| Workspace indexes | `StyioIDE` | `OpenFileIndex`, `BackgroundIndex`, `PersistentIndex` | C++ API | Query symbols and references across open files, background-indexed files, and cached symbol stores. |
| Semantic query database | `StyioIDE` | `styio::ide::SemanticDB` | C++ API | Provide cached syntax, semantic, HIR, completion, hover, definition, references, type, and semantic-token queries. |
| IDE facade | `StyioIDE` | `styio::ide::IdeService` | C++ API | Provide the primary in-process IDE service for document lifecycle, completion, hover, definition, references, symbols, semantic tokens, and runtime scheduling. |
| Runtime scheduling counters | `StyioIDE` | `RuntimeCounters`, `RuntimeIdleResult` | C++ API | Track request cancellation, stale drops, semantic diagnostic runs, background indexing, and latency counters. |
| LSP request handling | `StyioLSP` | `styio::lsp::Server::handle` | C++ API | Handle initialize, document sync, completion, hover, definition, references, symbols, semantic tokens, and runtime-drain requests through JSON-RPC payloads. |
| LSP diagnostic code mapping | `StyioLSP` | LSP `Diagnostic.code` and `data.phase` | LSP payload fields | Publish Styio diagnostic codes and phases to editor hosts without inventing a separate compiler grammar or diagnostic namespace. |
| LSP runtime drain | `StyioLSP` | `Server::drain_runtime` | C++ API | Publish queued semantic diagnostics and background work notifications after foreground requests. |
| LSP daemon | `StyioLSP` | `styio_lspd` / `Server::run` | CLI daemon plus C++ API | Run the editor-neutral stdio LSP server. |

## Contract Rules

1. New public services must be added to this manifest in the same change that introduces the source entrypoint.
2. Each module README must explain direct usage and link back to this manifest.
3. Consumer-specific docs may reference these capabilities, but must not redefine their contract shape.
4. Accepted Styio grammar is owned by the hand-written compiler parser. IDE or editor helpers must not introduce a separate grammar authority.
5. First-party adapters for Vityo, Spio, or hosted services may expose convenience payloads only when those payloads are backed by capabilities in this manifest or by a documented future capability state; they must not create private grammar, diagnostic, or semantic authorities.
