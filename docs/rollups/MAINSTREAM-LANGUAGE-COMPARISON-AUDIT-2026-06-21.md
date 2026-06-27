# Mainstream Language Comparison Audit 2026-06-21

**Purpose:** Record the source-architecture and compiler-algorithm audit comparing Styio with the local `lang-sources` checkouts of Rust, Go, CPython, Ruby, OCaml, and GHC, then define the optimization route for making Styio competitive with mainstream programming languages.

**Last updated:** 2026-06-25

**Status:** Active rollup. This is an evidence-backed planning audit, not a language semantic specification. Semantic decisions still belong in `docs/design/`, implementation inventories stay in the `IM-D*` rollups, and checkpoint execution stays governed by `workflows/`.

## Scope

This audit used four parallel read-only sub-agent reviews plus a main-thread source pass over:

| Area | Evidence read |
|------|---------------|
| Styio compiler | [src/CMakeLists.txt](../../src/CMakeLists.txt), [src/cmake/StyioFrontendSources.cmake](../../src/cmake/StyioFrontendSources.cmake), [src/cmake/StyioBackendSources.cmake](../../src/cmake/StyioBackendSources.cmake), [src/StyioParser](../../src/StyioParser), [src/StyioAST](../../src/StyioAST), [src/StyioSema](../../src/StyioSema), [src/StyioIR](../../src/StyioIR), [src/StyioLowering](../../src/StyioLowering), [src/StyioCodeGen](../../src/StyioCodeGen), [src/StyioRuntime](../../src/StyioRuntime), [src/StyioExtern](../../src/StyioExtern), [src/StyioNative](../../src/StyioNative), [src/StyioServices](../../src/StyioServices) |
| Styio quality surface | [tests](../../tests), [benchmark](../../benchmark), [workflows](../../workflows), [docs/rollups/CURRENT-STATE.md](./CURRENT-STATE.md), [docs/rollups/NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md) |
| External language sources | `<lang-sources-root>/rust`, `<lang-sources-root>/go`, `<lang-sources-root>/python`, `<lang-sources-root>/ruby`, `<lang-sources-root>/ocaml`, `<lang-sources-root>/ghc` |

The local external source directory is intentionally partial for some languages. The external `lang-sources/SOURCE_INDEX.md` notes that Ruby and OCaml are full working trees, while Go, Rust, CPython, and GHC are source-focused sparse checkouts. Conclusions below are limited to locally visible source structure and should not be read as exhaustive statements about every upstream file.

The initial audit was a static source and architecture review. Follow-up implementation work added automated gates and targeted tests for the direct baselines listed below.

## Executive Conclusion

Styio is not a toy compiler. It already has a real C++20/LLVM implementation, a layered source tree, a nightly parser authority contract, StyioIR, a verifier, codegen, native interop, IDE/LSP services, feature goldens, fuzz targets, security tests, and workflow documentation.

The gap is that mainstream languages are not just compilers. Rust, Go, CPython, Ruby, OCaml, and GHC all carry a language distribution system: compiler or interpreter, explicit intermediate representations, runtime, standard library, package/build tools, bootstrap path, diagnostics, cross-platform release machinery, long-lived regression suites, and performance baselines. Styio is currently closer to a strong experimental compiler repository than a complete language distribution.

The highest-leverage optimization path is not to add more surface syntax first. It is to harden the compiler/runtime architecture so that future features can be implemented with mainstream-language rigor:

1. Split AST, semantic facts, typed HIR, mid-level CFG/effect IR, and LLVM lowering into stable stages.
2. Replace string-shaped diagnostics with typed diagnostics that carry spans, labels, notes, expected/found data, and suggestions.
3. Split Sema into name binding, type inference/checking, resource/effect checking, and control-flow/return checking.
4. Turn runtime into a real subsystem instead of a large C ABI support file, with explicit concurrency and ownership rules.
5. Make `@extern` safe by design: argv-based compiler invocation, digest-backed artifacts, ABI contracts, allowlists, and sandbox/trust boundaries.
6. Define a release-grade distribution envelope: standard library, package handoff contract, bootstrap route, target/platform matrix, and reproducible benchmark core.

## Directly Comparable Engineering Baselines

The audit separates mature engineering baselines from Styio-specific language decisions. The items in this section are directly comparable to the local Rust, Go, CPython, Ruby, OCaml, and GHC source structures. They are not language-design choices unique to Styio; they are expected infrastructure for a serious language implementation.

| Baseline | Why it is directly comparable | Styio implication |
|----------|-------------------------------|-------------------|
| Structured diagnostics | Mature frontends expose stable diagnostic codes, spans, notes, expected/found facts, and machine-readable data for tools. | Parser, Sema, IR, runtime, native interop, CLI JSONL, and LSP diagnostics should share one typed diagnostic model. |
| Single parser authority plus grammar/tooling consistency tests | Mature language tooling avoids multiple accepted grammars drifting apart. | Nightly compiler parser remains authoritative; Tree-sitter and recovery syntax facts must be tested against compiler fixtures. |
| Syntax tree, semantic facts, and typed IR separation | Mature compilers separate syntax representation from type/symbol facts and later lowering stages. | AST should stay syntactic; `NodeId`, `TypeTable`, `SymbolTable`, typed HIR, and Styio-owned mid-level IR should carry semantic facts. |
| Pass manager, verifier, dumps, and timing | Mature compiler pipelines make optimization and invariant checks observable and repeatable. | Styio-owned IR needs registered passes, verifier-after-pass modes, IR dumps, pass timing, and optimization levels before relying only on LLVM. |
| Runtime subsystem boundaries | Mature language runtimes separate ownership, scheduler, errors, IO, allocation, and C ABI surfaces. | `StyioRuntime` should own handles, errors/effects, collections, tasks, strings, profiling, and resource lifetimes; `StyioExtern` should be a facade. |
| Native toolchain safety boundary | Mature FFI/native paths define ABI, symbols, artifacts, compiler invocation, and trust boundaries. | `@extern` should use argv execution, declared ABI/signature/symbol metadata, digest-backed artifacts, isolated cache paths, and explicit opt-in policy. |
| Standard library and distribution envelope | Mature languages ship a standard library, test surface, platform matrix, and release evidence. | Styio needs a first-class `library/` or `stdlib/` plan, release conformance gates, platform/target matrix, package handoff fixtures, and reproducible core benchmarks. |
| Long-lived regression and benchmark corpus | Mature language projects preserve behavior and performance evidence over time. | Styio should keep feature goldens, fuzz/security tests, and a minimal in-repo performance corpus even when deep benchmarking lives in `styio-benchmark`. |

These baselines can be planned as direct engineering closure work. They do not require choosing whether Styio is closer to Rust, Go, Python, Ruby, OCaml, or Haskell.

Follow-up implementation evidence now exists for these direct baselines:

1. Structured diagnostics: IR verifier failures use public `ir_verify` phase codes `STYIO_IR_VERIFY_CONTRACT` and `STYIO_IR_VERIFY_INACTIVE_NODE`.
2. Parser authority and AST/Sema/IR separation: existing parser-authority gates remain; product orchestration now enters Sema, lowering, and LLVM emission through `SemanticAnalysis`, `AstToStyioIRStage`, and `LLVMEmission`; and `architecture_layer_gate` prevents new cross-layer calls or includes outside approved AST, Sema, lowering, StyioIR, Codegen, and runtime boundaries.
3. Pass manager: `StyioIRPassManager` provides canonicalization pass registration, opt-level gating, verifier-before/after checks, timing, and optional IR dumps.
4. Runtime boundary: `StyioRuntime/RuntimeState.*` owns runtime error state and log-sink behavior behind the C ABI facade, and `runtime_surface_gate` locks ExternLib, ORC registration, and codegen helper drift.
5. Native interop: JIT/native artifact compilation, frontend self-calls, release archive/checksum subprocesses, and native executable linking run through argv/exec helpers instead of shell command execution.
6. Distribution and benchmark floor: `library/manifest.json`, `stdlib_manifest_gate`, Runtime install of library/prelude data, and `benchmark/core/` smoke coverage provide a repository-local standard-library and performance evidence floor.

## Styio-Specific Decision Areas

The items in this section should not be copied directly from one comparison language. They depend on Styio's own model: intent-aware stream processing, resource topology, symbolic syntax, explicit resource effects, algebraic absence, native-performance goals, and IDE-first usability.

| Decision area | Why it is Styio-specific | Required decision before broad implementation |
|---------------|--------------------------|----------------------------------------------|
| Resource/effect semantics | `?|`, named handlers, audited discard, cleanup effects, pressure observers, resource topology, and capability states are Styio language features. | Decide the exact capability, typestate, cleanup, fallback, handler, pressure, and block snapshot/commit rules in IM-D4 before widening runtime support. |
| Stream and task concurrency model | Styio's pulse/frame, zip, snapshot, task, and resource semantics are not the same as Go goroutines, GHC RTS threads, Ruby fibers, or CPython interpreter state. | Decide whether task execution can migrate/capture resource handles, which stream joins are deterministic, and how backpressure/timeout/failure are observed in IM-D5. |
| Mid-level IR semantic shape | A mid-level IR is a direct baseline, but Styio's IR must preserve resource effects, stream barriers, absence, and topology facts that LLVM cannot infer after lowering. | Decide the HIR-to-IR boundary, effect/resource operation representation, ownership/lifetime markers, and verifier responsibilities in IM-D1/IM-D4/IM-D5. |
| Type system growth path | Constraint solving is a mature compiler baseline, but Styio must choose how resource capabilities, fallback values, absence, matrix/container families, and future generics interact. | Decide which constraints are type equality, assignability, callable/indexable checks, resource capabilities, effect families, or absence/fallback constraints. |
| Runtime memory model | Mainstream options include ownership, GC, reference counting, arenas, and handle tables; Styio's resource and native goals may need a hybrid. | Decide handle ownership, cross-thread visibility, nested container release, string ownership, task capture, and native boundary lifetime rules. |
| JIT/AOT product balance | ORC JIT is current execution infrastructure, while mainstream compiled languages usually make AOT a primary product path. | Decide whether JIT remains the main development execution path, whether AOT is release-primary, and what shared backend contract both paths use. |
| Package ownership split | `styio` intentionally delegates full package-manager UX to `styio-spio`, unlike Go or Rust where the main distribution includes more tool lifecycle. | Decide the minimum manifest/import/build graph and lockfile handoff contracts that must still be tested inside `styio`. |
| Bootstrap route | Self-hosting is a mainstream maturity marker, but Styio needs a subset and staged route that match current C++/LLVM implementation reality. | Decide the stage0/stage1/stage2 boundary, which Styio subset can compile standard library/tooling first, and which artifacts remain C++-hosted. |
| Parser technology | A single authoritative parser is a direct baseline; the exact hand-written, generated grammar, PEG, Pratt, or hybrid strategy is a Styio maintenance decision. | Decide how to preserve symbolic syntax, recovery quality, Tree-sitter consistency, and future formatter/doc tooling without creating parallel grammar truths. |

These decisions should be recorded in the owning `IM-D*` inventory or design SSOT before implementation expands. The audit should not present a mainstream-language mechanism as automatically correct for these Styio-specific surfaces.

## Mainstream Common Shape

The six comparison languages differ deeply, but their repositories show the same structural lesson.

| Language | Local evidence | Architecture lesson for Styio |
|----------|----------------|-------------------------------|
| Rust | `compiler/rustc_*`, `compiler/rustc_ast`, `rustc_hir`, `rustc_mir_*`, `rustc_codegen_llvm`, `library/core`, `library/alloc`, `library/std`, `x.py` | Make each compiler phase and library tier a separately owned component. Stable HIR/MIR-like representations give type checking, optimization, borrow/effect checks, and tooling durable boundaries. |
| Go | `src/cmd/compile/internal/{syntax,types,ssa}`, `src/runtime`, `src/cmd/go`, `src/cmd/link`, `src/testing`, standard packages under `src` | Treat compiler, runtime, standard library, formatter/build tool, testing, and linker as one distribution. SSA pass infrastructure and architecture lowering are first-class. |
| CPython | `Parser`, `Python`, `Objects`, `Modules`, `Include`, `Tools/cases_generator`, `Programs` | Separate parser, bytecode compiler, VM/object model, public/internal C API, extension modules, and generated interpreter cases. Diagnostics and parser recovery are product surfaces. |
| Ruby | `parse.y`, `prism`, `vm*.c`, `gc`, `jit`, `yjit`, `zjit`, `ext`, `lib`, `test`, `spec` | Keep parser evolution, VM, GC, JIT tiers, C extensions, standard libraries, and compatibility specs visible in the main language source. |
| OCaml | `parsing`, `typing`, `lambda`, `bytecomp`, `middle_end/flambda`, `asmcomp`, `runtime`, `stdlib`, `boot`, `testsuite` | Use explicit compiler stages and typed trees; keep bootstrap assets, runtime, standard library, bytecode/native backends, and tests as core architecture. |
| GHC | `compiler/GHC/{Parser,Tc,Core,Stg,Cmm,CmmToLlvm}`, `rts`, `libraries`, `nofib` | Maintain multiple semantic IR levels and a real runtime system; keep performance corpus and libraries close enough to shape compiler decisions. |

Styio already has corresponding names for many of these layers, but several are still too thin or too coupled:

| Mainstream component | Styio today | Gap |
|----------------------|-------------|-----|
| Stable frontend pipeline | Tokenizer, hand parser, AST, Sema, Tree-sitter IDE backend, parser authority gates, stage-entry APIs, `architecture_layer_gate` | Product orchestration no longer calls AST/Sema/IR visitor hooks directly, but AST nodes still expose compatibility visitor methods while typed HIR and side-table migration proceeds. |
| Typed semantic layer | `StyioSemaContext`, node data types, resource topology checks | No independent `TypeTable`, `SymbolTable`, typed HIR, constraint engine, or error trace system. |
| Mid-level optimizer IR | `StyioIR`, `StyioIROptimizer`, verifier | IR nodes directly expose LLVM codegen; no explicit CFG/SSA/effect/ownership mid-level contract. |
| Runtime system | `StyioExtern/ExternLib.cpp`, `StyioRuntime/HandleTable.hpp`, `StyioRuntime/RuntimeState.*`, task support, `runtime_surface_gate` | Runtime state has started moving below the C ABI facade; collections, tasks, strings, files, and profiling still need subsystem decomposition. |
| Standard library | `library/manifest.json`, `library/*/README.md`, and installed `src/StyioPrelude/resources.styio` compatibility prelude | Planned modules are reserved, but only `std.resource` is active; compiler intrinsics and examples are adjacent evidence, not standard-library APIs. Package-aware import and accepted APIs remain future work. |
| Package/build distribution | Compiler-side nano and compile-plan contracts | Full package lifecycle is intentionally outside `styio`, but the main repo still needs stable manifest/import/build graph contracts and tests. |
| Performance corpus | External `styio-benchmark`, adapters, and `benchmark/core/` smoke corpus | The main repo has a release-conformance timing-schema floor; deep comparisons and historical baselines remain external. |

## Frontend And Diagnostics

Styio's parser is a hand-written recursive-descent implementation with strict/recovery modes, nesting budgets, statement-boundary recovery, and a declared nightly authority path. That is a reasonable strategy: Go and Rust also use hand-written parser code for important parts of the frontend.

The weakness is not "hand-written parser". The weakness is the lack of structured frontend facts.

| Area | Styio evidence | Mainstream contrast | Risk |
|------|----------------|---------------------|------|
| Parser authority | [src/StyioParser/Parser.hpp](../../src/StyioParser/Parser.hpp) still carries `Legacy`, `Nightly`, and migration counters. | Rust/Go parser APIs expose structured result/error flows; CPython PEG has memoization and a second error pass; Ruby Prism exposes parse errors/warnings as API facts. | New syntax can accidentally preserve migration paths or diverge between compiler parser and editor grammar. |
| Diagnostics | `StyioParseDiagnostic` is span/message-shaped, while many Sema failures throw string-bearing exceptions. | Rust/GHC/OCaml diagnostics carry typed codes, expected/found values, labels, notes, and traces. | The compiler can be correct but hard to use; IDE and CLI cannot reliably classify or auto-fix failures. |
| AST ownership of semantic work | [src/StyioAST/AST.hpp](../../src/StyioAST/AST.hpp) still gives nodes compatibility `typeInfer` and `toStyioIR` hooks, while product orchestration enters through [src/StyioSema/SemanticAnalysis.hpp](../../src/StyioSema/SemanticAnalysis.hpp) and [src/StyioLowering/AstToStyioIRStage.hpp](../../src/StyioLowering/AstToStyioIRStage.hpp). | Rust separates AST/HIR/MIR; OCaml separates Parsetree/Typedtree; Go stores type facts in `types.Info`. | The immediate orchestration knot is contained, but later generics/effects/resource checking still need typed HIR and side tables to remove semantic ownership from AST nodes. |
| IDE syntax | Tree-sitter incremental parsing exists under [src/StyioServices/StyioIDE](../../src/StyioServices/StyioIDE). | Mature IDE stacks reuse compiler-owned semantic facts or maintain generated grammar consistency gates. | IDE can become a separate grammar authority unless consistency tests force alignment. |

P0 frontend direction:

1. Define a single `Diagnostic` data model with `code`, `severity`, `phase`, `primary span`, `labels`, `notes`, `expected`, `found`, and `suggestions`.
2. Introduce stable `NodeId` allocation and side tables: `TypeTable`, `SymbolTable`, `DefUse`, and source map facts.
3. Keep AST as syntax, then lower to typed HIR before resource/effect and IR work.
4. Make nightly the only production parser truth; Tree-sitter must be tested against compiler parser fixtures, not treated as a parallel accepted grammar.
5. Add parser recovery golden tests that prove multiple diagnostics after malformed input, not only first-error failure.

## Type System And Sema

Styio's type inference is currently rule-driven and local: expression visitors compute `StyioDataType`, Sema stores local maps, and mismatches throw. This has been enough to advance scalar, container, resource, task, and matrix slices, but it is not the structure that mainstream type systems use once generics, effect typing, resource capabilities, and IDE assistance become central.

| Need | Current shape | Target shape |
|------|---------------|--------------|
| Name resolution | `StyioSemaContext` owns function definitions, local binding maps, native signatures, and resource method state. | Separate name-binding pass producing scopes, symbols, import facts, and def-use edges. |
| Type inference | Recursive expression checks and type-family helpers. | Constraint generation plus unification/solving, with source-located constraint origins. |
| Resource/effect typing | Resource topology checks exist, but many feature slices still have local routing. | Capability/effect constraints integrated with type solving and verified again in mid-level IR. |
| Error quality | Broad `StyioTypeError` or mapped diagnostic families. | Expected/found traces tied to the expression, call argument, assignment, fallback, resource op, or handler that created the constraint. |
| IDE facts | HIR/SemDB exists but is not yet the single semantic database. | Incremental semantic database over VFS, syntax tree, HIR, symbols, types, and diagnostics. |

P0/P1 Sema direction:

1. P0: Split `StyioSemaContext` into passes: name binding, type checking/inference, resource/effect checking, return/control-flow checking.
2. P0: Stop adding new semantic state directly to AST node classes unless the AST fact is truly syntactic.
3. P1: Add `TypeVar`, `Constraint`, `Unifier`, `Solution`, and `ErrorTrace` for variables, calls, containers, binary ops, fallbacks, and resource capabilities.
4. P1: Move IDE hover/completion/definition onto the same HIR/type facts used by CLI checks.

## IR, Optimization, And Backend

Styio has real IR and a verifier, but it is not yet a mainstream mid-level compiler architecture. [src/StyioIR/StyioIR.hpp](../../src/StyioIR/StyioIR.hpp) gives IR nodes `toLLVMType` and `toLLVMIR`, which means the "middle" representation already knows too much about the final backend. Product orchestration now calls [src/StyioCodeGen/LLVMEmission.hpp](../../src/StyioCodeGen/LLVMEmission.hpp), so backend entry is centralized, but the node-level compatibility hook remains until a deeper backend interface migration lands. [src/StyioLowering/StyioIROptimizer.cpp](../../src/StyioLowering/StyioIROptimizer.cpp) contains useful local canonicalization, and LLVM runs InstCombine, Reassociate, GVN, and SimplifyCFG, but high-level Styio semantics are often gone before LLVM can exploit them.

Mainstream compilers avoid this by preserving optimization-friendly structure:

| Reference | Relevant pattern |
|-----------|------------------|
| Rust | HIR, MIR, MIR dataflow, MIR transform pass registry, LLVM/codegen backends. |
| Go | SSA package with pass list, invariant checking, dumps, architecture lowering, rewrite rules. |
| OCaml | Lambda, Flambda, Clambda/Cmm stages with explicit optimization and closure conversion. |
| GHC | Core, STG, Cmm, LLVM/native backends, runtime-aware lowering. |
| CPython/Ruby | Bytecode plus tiered optimizer/JIT IR for dynamic execution paths. |

P0 backend direction:

1. Define a typed High/Mid IR between AST/HIR and LLVM.
2. Include explicit basic blocks, terminators, resource/effect operations, ownership/lifetime markers, and typed values.
3. Move LLVM emission out of generic `StyioIR` nodes and behind a backend interface.
4. Build a pass pipeline with registered passes, verifier stages, pass timing, dump controls, and optimization levels.
5. Add at least these first passes: CFG verifier, type verifier, resource lifetime verifier, local DCE, constant folding, effect-aware hoist guard, collection/matrix intrinsic lowering.

This does not require deleting the current StyioIR immediately. The practical migration is to add the new IR behind the existing accepted language slices, then move feature families one by one with golden dumps and verifier tests.

## Runtime, Memory, And Concurrency

The highest-risk runtime finding in this audit is the remaining `thread_local` boundary: the runtime still stores handles, owned strings, and several resource families in `thread_local` structures inside [src/StyioExtern/ExternLib.cpp](../../src/StyioExtern/ExternLib.cpp), while the task runtime uses worker threads. Runtime error state and the log sink now live in [src/StyioRuntime/RuntimeState.cpp](../../src/StyioRuntime/RuntimeState.cpp), but source-level task/resource semantics still need an explicit confinement or migration decision for the other runtime state families.

[src/StyioRuntime/HandleTable.hpp](../../src/StyioRuntime/HandleTable.hpp) and [src/StyioRuntime/RuntimeState.cpp](../../src/StyioRuntime/RuntimeState.cpp) are the first runtime subsystem pieces. The current runtime surface still lives mostly in `ExternLib.cpp`, which mixes file IO, lists, dicts, matrices, tasks, string ownership, profiling, and symbol registration.

Mainstream runtimes make this boundary explicit:

| Runtime | Pattern |
|---------|---------|
| Go | Scheduler, GC, stack/runtime metadata, channels, timers, netpoll, system calls. |
| GHC | RTS capabilities, threads, scheduler, storage manager, event log. |
| CPython | Object model, refcount/GC, interpreter state, modules, C API, allocator. |
| Ruby | VM, GC, fibers/ractors, JIT hooks, extension loading. |
| OCaml | Runtime allocation, domains, GC, C interface. |

P0 runtime direction:

1. Define whether Styio task execution can access or migrate resource handles. If not, reject cross-task resource capture statically. If yes, make handles globally owned and concurrency-safe.
2. Move runtime ownership out of incidental `thread_local` state unless the type system proves thread-local confinement.
3. Split `StyioRuntime` into owned modules: handles, errors/effects, strings, collections, matrices, tasks/scheduler, file resources, profiling, and allocation accounting.
4. Leave `StyioExtern` as a C ABI facade over `StyioRuntime`, not the implementation home.
5. Add leak, double-release, cross-thread, failed-cleanup, and task_capture tests as runtime contract tests.

## Native Interop

Styio's `@extern(c|c++)` path is valuable because it lets the language leverage mature native ecosystems. The current implementation compiles external code and loads a shared object, with signature extraction, native toolchain configuration, and argv/exec subprocess execution already present.

The risk is that this is a security and supply-chain boundary. Shell command execution has been removed from the main native/release subprocess paths, but the broader trust model still needs artifact digests, cache isolation, and explicit provenance.

P0 native interop direction:

1. Keep argv-based process execution as the subprocess baseline and reject regressions to shell command construction.
2. Require explicit ABI, language, compiler mode, allowed flags, source digest, output digest, and symbol list in native artifact metadata.
3. Isolate native build cache and load paths.
4. Add an allowlist or explicit project opt-in for native compilation.
5. Generate one registry for runtime symbols and native intrinsic declarations so JIT symbol maps, headers, and codegen lookup cannot drift.

This maps directly to [IM-D7-NATIVE-INTEROP-ABI-INVENTORY.md](./IM-D7-NATIVE-INTEROP-ABI-INVENTORY.md).

## Toolchain, Standard Library, And Distribution

Styio already has a nontrivial test and workflow surface for an experimental language repository: feature goldens, parser/pipeline cases, fuzz targets, security tests, IDE tests, docs gates, coverage gates, and external benchmark adapters.

The gap is distribution completeness:

| Distribution area | Current Styio state | Competitive-language target |
|-------------------|---------------------|-----------------------------|
| Standard library | `library/manifest.json` skeleton plus compatibility `std.resource` source in `src/StyioPrelude/resources.styio`; current matrix and series helper surfaces are compiler intrinsics, not `std.*` APIs. | Grow the first-class `library/` tree with accepted `core`, `io`, `collections`, `text`, `math`, `stream`, `resource`, `time`, `path`, `process`, `test`, docs, and module tests. |
| Package/build | Nano package producer and compile-plan handoff; full package lifecycle outside this repo. | Versioned manifest, import graph, build graph, lockfile/handoff schema, package negative tests, `spio` contract tests. |
| Tooling | CLI, syntax check, build, IDE/LSP, compile-plan, nano paths. | `styio fmt`, `styio test`, `styio doc`, `styio check`, stable LSP, formatter/doc extraction, project workspace model. |
| Bootstrap | CMake builds C++ implementation; nano profile exists. | Explicit stage0/stage1/stage2 route, even if Styio self-hosting begins with a subset. |
| CI/platforms | Local gates and GitHub workflows exist. | Linux/macOS/Windows, sanitizer matrix, release artifacts, target triple/sysroot/linker policy, checksums/SBOM/provenance. |
| Benchmarks | External `styio-benchmark`, local adapters, and `benchmark/core/` smoke corpus. | Keep the minimal in-repo reproducible performance core green while external deep reports own comparisons and baselines. |

P0 distribution direction:

1. Keep the first-class `library/` manifest/directory shape gated while moving active prelude content out of `src/` when package/import support is ready.
2. Keep full package-manager UX outside `styio`, but make manifest/import/build graph contracts testable in this repo.
3. Keep the minimal `benchmark/core/` performance corpus in this repo so core executable timing evidence is reproducible without external checkout availability.
4. Define the staged bootstrap route before large-scale self-hosting work begins.
5. Expand release conformance work through [IM-D6-RELEASE-CONFORMANCE-INVENTORY.md](./IM-D6-RELEASE-CONFORMANCE-INVENTORY.md), [IM-D8-STDLIB-DOMAIN-LIBRARY-INVENTORY.md](./IM-D8-STDLIB-DOMAIN-LIBRARY-INVENTORY.md), and [IM-D10-PACKAGE-MODULE-COMPATIBILITY-INVENTORY.md](./IM-D10-PACKAGE-MODULE-COMPATIBILITY-INVENTORY.md).

## Prioritized Optimization Plan

### P0: Architecture Stop Lines

These should block broad new language-feature expansion unless the feature directly helps close the stop line.

| Stop line | Classification | Required result | Owning inventory |
|-----------|----------------|-----------------|------------------|
| Typed diagnostics | Direct baseline | Parser/Sema/IR/runtime/native diagnostics use stable structured facts instead of ad hoc strings; IR verifier now reports public `STYIO_IR_VERIFY_*` codes. | [IM-D3](./IM-D3-DIAGNOSTIC-CONTRACT-INVENTORY.md) |
| Parser authority and grammar consistency | Direct baseline, Styio parser technology decision | Nightly remains the compiler authority; Tree-sitter and recovery parser facts are tested against compiler fixtures. | [IM-D2](./IM-D2-PARSER-AUTHORITY-INVENTORY.md), [IM-D9](./IM-D9-IDE-LSP-SERVICE-CONTRACT-INVENTORY.md) |
| Typed semantic facts | Direct baseline | AST is syntax; type/symbol/def-use facts live in side tables and typed HIR. `architecture_layer_gate` prevents new coupling while the deeper side-table/HIR migration proceeds. | [IM-D1](./IM-D1-STYIOIR-CONTRACT-INVENTORY.md), [IM-D3](./IM-D3-DIAGNOSTIC-CONTRACT-INVENTORY.md) |
| Mid-level IR | Direct baseline, Styio semantic-shape decision | Add CFG/effect/resource/lifetime-aware IR with verifier and pass pipeline before LLVM. | [IM-D1](./IM-D1-STYIOIR-CONTRACT-INVENTORY.md), [IM-D4](./IM-D4-RESOURCE-MANAGEMENT-INVENTORY.md), [IM-D5](./IM-D5-STREAM-CONCURRENCY-INVENTORY.md) |
| Runtime concurrency | Styio-specific decision on top of direct runtime-boundary baseline | `RuntimeState` is separated; resolve remaining thread-local handle/string/container/task semantics vs worker-thread execution. | [IM-D4](./IM-D4-RESOURCE-MANAGEMENT-INVENTORY.md), [IM-D5](./IM-D5-STREAM-CONCURRENCY-INVENTORY.md) |
| Native interop safety | Direct baseline | argv execution is implemented; digest metadata, ABI declarations, cache isolation, and trust boundaries remain. | [IM-D7](./IM-D7-NATIVE-INTEROP-ABI-INVENTORY.md) |
| Distribution floor | Direct baseline, Styio package-boundary decision | Define first-class standard library, package handoff schema, minimal benchmark corpus, bootstrap route, and platform matrix. | [IM-D6](./IM-D6-RELEASE-CONFORMANCE-INVENTORY.md), [IM-D8](./IM-D8-STDLIB-DOMAIN-LIBRARY-INVENTORY.md), [IM-D10](./IM-D10-PACKAGE-MODULE-COMPATIBILITY-INVENTORY.md) |

### P1: Industrialization

1. Expand the new StyioIR pass manager beyond canonicalization: CFG/type/resource verifiers, memory stats, and effect-aware optimization passes.
2. Continue splitting `StyioRuntime` from `StyioExtern`: collections, strings, files, tasks, profiling, and allocation modules below the C ABI facade.
3. Generate runtime/JIT/native symbol tables from one declaration source.
4. Add constraint-based type inference for bindings, calls, containers, binary ops, resource effects, and fallbacks while preserving `architecture_layer_gate`.
5. Add IDE protocol goldens for hover/completion/definition/references/diagnostics/semantic tokens, then extend to rename/codeAction/inlayHint only after semantic facts are unified.
6. Expand sanitizer, fuzz, and benchmark matrices so they run as routine quality gates, not only ad hoc deep checks.

### P2: Ecosystem And Ergonomics

1. Build sample workspaces: library project, binary project, package dependency, native interop project, long-running stream service, and multi-file module import project.
2. Add formatter/doc/comment trivia preservation so Styio can support `styio fmt` and `styio doc` without re-parsing approximations.
3. Add edition/compatibility suites so accepted language behavior can evolve intentionally rather than by accidental nightly breakage.
4. Document public compiler/service APIs separately from internal headers and implementation classes.
5. Keep the external `styio-benchmark` deep suite, but promote durable headline reports and core workloads into release conformance.

## Strategic Guidance

Styio's differentiator should remain its own language model: intent-aware stream processing, resource topology, explicit resource effects, pulse/snapshot semantics, native performance, and high-quality IDE support. Competing with mainstream languages does not mean copying any one of them.

It does mean adopting their engineering discipline:

1. Every accepted syntax form should have parser, semantic, IR, runtime/codegen, diagnostic, IDE, and test ownership.
2. Every performance statement should have a reproducible workload and baseline.
3. Every resource or native boundary should have an explicit safety model.
4. Every distribution promise should have install/build/test/release evidence.
5. Every migration bridge should carry an exit condition.

Styio should keep its performance-first, usability-second governance from [PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md). The audit conclusion is that the next performance and usability gains come from architectural consolidation, not from adding more isolated features on top of the current coupling.

## Next Checkpoint Entry

Use this audit to seed checkpoint-sized work, not a monolithic rewrite.

| First checkpoint | Minimal artifact | Proof |
|------------------|------------------|-------|
| Diagnostic model P0 | Extend the shared diagnostic taxonomy with expected/found payloads for one sema/type family | CLI JSONL, LSP diagnostic, and negative golden all share the same code/span/expected/found facts |
| Type side table P0 | `NodeId`, `TypeTable`, and `SymbolTable` for bindings and binary expressions | Existing scalar/function feature tests still pass; AST no longer needs new semantic fields for that slice |
| Mid-level IR P0 | CFG/effect IR prototype for scalar bind, call, branch, return, and resource write | IR dump golden, verifier failure test, LLVM lowering equivalence |
| Runtime state P0 | Decision and first implementation for task/resource handle confinement or migration | Cross-task handle/resource tests prove either static rejection or safe execution |
| Native interop P0 | argv-based compiler invocation and artifact digest metadata | Security test covers shell metacharacters, cache mismatch, and missing declared symbol |
| Distribution floor P0 | Grow accepted modules beyond the current `library/` manifest and `std.resource` compatibility prelude; keep `stdlib_manifest_gate`, install-tree validation, and `benchmark/core` smoke green | Docs, CMake/package handoff, feature tests, stdlib manifest validation, install data, and core benchmark JSON evidence agree on module/performance ownership |
