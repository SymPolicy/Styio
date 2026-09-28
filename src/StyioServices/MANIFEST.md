# StyioServices Manifest

**Purpose:** List every public service capability currently available under `src/StyioServices/`.

**Last updated:** 2026-05-14

## Capability Inventory

| Capability | Module | Entry Point | Current Form | Use |
|------------|--------|-------------|--------------|-----|
| Syntax-only JSON check | `StyioCLI` | `styio check --syntax --json --file <path>` / `styio::services::run_syntax_check_cli` | CLI contract | Validate Styio source syntax without type checking, lowering, codegen, execution, or runtime resource access. |
| Parser engine selection for syntax check | `StyioCLI` | `--parser-engine nightly|legacy` | CLI option | Select the parser engine used by syntax-only validation. |
| Machine info contract | `StyioConfig` via `src/main.cpp` | `styio --machine-info=json` | CLI JSON contract | Discover compiler version, release channel, supported contracts, feature flags, adapter modes, and capabilities. |
| Compile-plan contract parsing | `StyioConfig` | `styio::config::parse_compile_plan` / `styio --compile-plan <path>` | C++ helper plus CLI contract | Parse and validate versioned compiler request envelopes for build, check, run, and test handoff. |
| Compile-plan diagnostics directory probing | `StyioConfig` | `styio::config::probe_compile_plan_diag_dir` | C++ helper | Find the requested diagnostics directory early enough for machine-readable CLI errors. |
| Build-mode vocabulary | `StyioConfig` | `default_build_mode_name`, `is_supported_build_mode` | C++ helper | Keep source-build and compile-plan build-mode handling centralized. |
| Source-build info contract | `StyioConfig` | `styio::config::source_build_info_json` / `styio --source-build-info=json` | C++ helper plus CLI JSON contract | Publish official source origin, source channel mapping, public build entrypoints, and controlled source components. |
| Nano profile feature macros | `StyioConfig` | `NanoProfile.hpp` | Compile-time configuration | Publish full/nano feature flags and pruning controls to shared compiler/runtime code. |
| URI and text-position primitives | `StyioIDE` | `Common.hpp` | C++ API | Convert URI/path values, map offsets to positions, and share editor-facing data structures. |
| Virtual file system snapshots | `StyioIDE` | `styio::ide::VirtualFileSystem` | C++ API | Open, update, incrementally edit, close, and snapshot documents. |
| Syntax snapshots | `StyioIDE` | `styio::ide::SyntaxParser` | C++ API | Produce tolerant or Tree-sitter-backed token, node, diagnostic, matching-token, and folding snapshots. |
| Compiler semantic bridge | `StyioIDE` | `styio::ide::analyze_document` | C++ API | Reuse the compiler parser/type facts for IDE semantic summaries and diagnostics. |
| HIR construction | `StyioIDE` | `styio::ide::HirBuilder` | C++ API | Build stable editor-facing item, scope, symbol, and reference models from syntax and semantic facts. |
| Workspace indexes | `StyioIDE` | `OpenFileIndex`, `BackgroundIndex`, `PersistentIndex` | C++ API | Query symbols and references across open files, background-indexed files, and cached symbol stores. |
| Semantic query database | `StyioIDE` | `styio::ide::SemanticDB` | C++ API | Provide cached syntax, semantic, HIR, completion, hover, definition, references, type, and semantic-token queries. |
| IDE facade | `StyioIDE` | `styio::ide::IdeService` | C++ API | Provide the primary in-process IDE service for document lifecycle, completion, hover, definition, references, symbols, semantic tokens, and runtime scheduling. |
| Runtime scheduling counters | `StyioIDE` | `RuntimeCounters`, `RuntimeIdleResult` | C++ API | Track request cancellation, stale drops, semantic diagnostic runs, background indexing, and latency counters. |
| LSP request handling | `StyioLSP` | `styio::lsp::Server::handle` | C++ API | Handle initialize, document sync, completion, hover, definition, references, symbols, semantic tokens, and runtime-drain requests through JSON-RPC payloads. |
| LSP runtime drain | `StyioLSP` | `Server::drain_runtime` | C++ API | Publish queued semantic diagnostics and background work notifications after foreground requests. |
| LSP daemon | `StyioLSP` | `styio_lspd` / `Server::run` | CLI daemon plus C++ API | Run the editor-neutral stdio LSP server. |

## Contract Rules

1. New public services must be added to this manifest in the same change that introduces the source entrypoint.
2. Each module README must explain direct usage and link back to this manifest.
3. Consumer-specific docs may reference these capabilities, but must not redefine their contract shape.

