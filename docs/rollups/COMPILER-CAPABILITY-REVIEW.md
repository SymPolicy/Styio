# Compiler Capability Review

**Purpose:** Summarize verified compiler structure, current capability limits and the owning evidence for further engineering work; use feature contracts for language meaning and the gap ledger for delivery priorities.

**Last updated:** 2026-10-01

## Scope

This review describes the current source tree and registered evidence. A source
or fixture reference is not a fresh execution result. Build, runtime, performance
and cross-repository acceptance must identify the tested revision and command.

Compiler layers remain independently owned. A proposed typed representation,
runtime decomposition or inference extension requires its own interface and
acceptance criteria; the presence of similar components in another compiler
does not by itself require Styio to adopt that architecture.

## Current Structure and Remaining Work

| Surface | Current evidence | Remaining boundary and owner |
|---|---|---|
| Compiler stages | [SemanticAnalysis](../../src/StyioSema/SemanticAnalysis.hpp), [AstToStyioIRStage](../../src/StyioLowering/AstToStyioIRStage.hpp), and [LLVMEmission](../../src/StyioCodeGen/LLVMEmission.hpp) provide stage entrypoints. CMake target composition is owned by [src/CMakeLists.txt](../../src/CMakeLists.txt). | AST/IR compatibility visitor hooks remain. A compiler-wide typed HIR or backend-independent node API is a separate migration; [IM-D1](./IM-D1-STYIOIR-CONTRACT-INVENTORY.md) owns the accepted IR boundaries. |
| Semantic identity and type storage | Session-owned [TypeTable](../../src/StyioSession/TypeTable.hpp) and [SymbolInterner](../../src/StyioSession/SymbolInterner.hpp) are integrated into selected [SemaContext](../../src/StyioSema/SemaContext.hpp) paths, with evidence in [typeinfer_internal_test.cpp](../../tests/typeinfer_internal_test.cpp). | Type-object/string fallback paths remain. Symbol interning is not a complete symbol/def-use database, and IDE HIR is not a compiler-wide typed HIR. |
| Callable inference | [TypeInfer.cpp](../../src/StyioSema/TypeInfer.cpp) contains an occurs-check unifier, rank-1 schemes, recursive-group analysis, context-driven instantiation and a closed constraint worklist. [Inferred-generics fixtures](../../tests/features/inferred_generics/t05_generic_composition.styio) and internal tests cover these slices. | Generalization remains closed/pure/final and capability-scoped. Rank-2 callbacks and user-extensible constraints remain deferred under their feature contracts. Broader checking still uses feature-specific visitors and helpers. |
| Resource topology | Parser, Sema and lowering accept current resource declarations and bounded state/snapshot slices; [resource_topology_test.cpp](../../tests/resource_topology_test.cpp) and [state-resource fixtures](../../tests/features/state_resources/t06_topology_selector_snapshot.styio) record acceptance and rejection. | Unsupported selector/shape combinations and additional driver families require individual accepted slices. Resource topology is not a complete program/control-flow graph. |
| StyioIR and optimization | The verifier and [PassManager](../../src/StyioLowering/StyioIROptimizer.hpp) provide pass registration, verification and timing for the implemented transformations. | Broader CFG, effect-aware and lifetime-aware optimization requires explicit representations and legality evidence. Native LLVM emission hooks remain a known interface boundary. |
| Runtime | [RuntimeState](../../src/StyioRuntime/RuntimeState.hpp), handles, scheduling and observation support have explicit modules; the runtime-surface gate checks exported helpers against ORC/codegen use. | Collection, string, file and native-boundary implementations still share larger C ABI support surfaces. Thread confinement, handle transfer and further module decomposition follow [IM-D4](./IM-D4-RESOURCE-MANAGEMENT-INVENTORY.md) and [IM-D5](./IM-D5-STREAM-CONCURRENCY-INVENTORY.md). |
| IDE/LSP | [Server.cpp](../../src/StyioServices/StyioLSP/Server.cpp) integrates budgeted diagnostic draining and request-driven background work; [IDE tests](../../tests/ide/styio_ide_test.cpp) cover the loop. | Current methods and limits are listed in [LSP.md](../external/for-ide/LSP.md). Local/single-workspace behavior and the limited rename/codeAction/inlayHint contracts remain explicit; these methods and drain behavior are implemented, while broader refactor support requires separate evidence. |
| Native interop | [NativeInterop](../../src/StyioNative/NativeInterop.cpp) implements C/C++ compilation and C-ABI interop; subprocess invocation uses the owned process boundary. | Supported signatures, pointer/string mapping, host toolchain and artifact validation remain bounded by [IM-D7](./IM-D7-NATIVE-INTEROP-ABI-INVENTORY.md). Native access is not a general zero-copy buffer API. |
| Library and distribution | [library/manifest.json](../../library/manifest.json) has `std.resource` active; its prelude is installed from `share/styio/prelude/`. Compile-plan and native artifact contracts have their own tests. | Other standard-library entries are planned. Package workflow and IDE UI remain externally owned; [IM-D6](./IM-D6-RELEASE-CONFORMANCE-INVENTORY.md), [IM-D8](./IM-D8-STDLIB-DOMAIN-LIBRARY-INVENTORY.md), and [IM-D10](./IM-D10-PACKAGE-MODULE-COMPATIBILITY-INVENTORY.md) define their boundaries. |
| Performance evidence | [benchmark/README.md](../../benchmark/README.md) documents the explicit optional CMake integration with `styio-benchmark`. The compiler supplies probes and observation/budget interfaces. | Workloads, runners, baselines and reports live in the external benchmark repository. There is no active repository-local `benchmark/core/` corpus. |

## Inference Work Entry

Start from the accepted feature authorities:

- [Inferred callable relations](../design/syntax/features/core-inferred-callable-relation.md)
- [Context-driven instantiation](../design/syntax/features/core-context-driven-call-instantiation.md)
- [Recursive callable groups](../design/syntax/features/core-recursive-callable-group.md)
- [Closed callable constraints](../design/syntax/features/core-constrained-callable-relations.md)
- [Fixed defaults](../design/syntax/features/core-fixed-inference-defaults.md)
- [Effect-aware generalization](../design/syntax/features/core-effect-aware-callable-generalization.md)

Further work can investigate constraint-origin diagnostics, propagation within
a single expression and removal of fallback representations. It must preserve
the existing closed/pure/usage/defaulting rules or explicitly revise their
owning feature. The [research agenda](../design/Styio-Research-Innovations.md#62-formal-verification)
proposes a scoped formal model; no proof of the C++ implementation follows from
the existing test suite or observable evidence graph.

## Choosing the Next Checkpoint

1. Select one open item from [NEXT-STAGE-GAP-LEDGER](./NEXT-STAGE-GAP-LEDGER.md).
2. Verify its current source path, capability and rejection behavior before
   describing it as missing. Replace obsolete broad claims with a concrete gap.
3. Name the owner contract, affected producer/consumer and smallest positive and
   negative acceptance cases.
4. For structural work, preserve existing target dependencies, symbol exports,
   accepted behavior and unsupported-path diagnostics unless a separate design
   change is approved.
5. Use the corresponding runbook and [Test Catalog](../../workflows/TEST-CATALOG.md)
   for commands. Architecture gates enforce their coded checks; independent
   consumer build/link tests cover additional interface boundaries.
6. Keep old comparison narratives and completed task lists in Git history.
   Maintain current facts here and active implementation sequencing in its plan.
