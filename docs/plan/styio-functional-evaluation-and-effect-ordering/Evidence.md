# Styio Functional Evaluation and Effect Ordering Evidence

**Purpose:** Record the accepted semantic authority, current repository implementation gaps, comparable compiler practice, and migration evidence required before Q03-F source work.

**Last updated:** 2026-07-20

## Authority and status

| Evidence | Repository fact | Planning consequence |
|---|---|---|
| `docs/design/Styio-Functional-Evaluation-and-Effect-Ordering.md` | Accepted Q03-F defines strict values, dependency-only safe-pure siblings, explicit order edges, conflict rejection, Block completion stop, exact-once inputs, lazy CFG, four optimization rights, and runtime-free lowering. | Current implementation behavior is evidence of gaps, never semantic authority. |
| `docs/design/Styio-Language-Decision-Ledger.md` (`Q03-F`) | The decision is registered as accepted and links the focused owner. | This lifecycle may implement but not reopen Q03-F. |
| `docs/design/Styio-Operation-Completion-and-Settlement.md` and the directional-flow plan | Q01-A owns finite nominal completion families, `OperationSummary(success_type, completion_set)`, canonical directional/settlement shape and matching, and deletion of legacy direction forks. | Q03-F must keep `OperationSummary` unchanged, use independent `EvaluationFacts`, and wait for the Q01 canonical interface instead of duplicating it. |
| `docs/design/Styio-Callable-Principal-Inference.md` | Q02 represents callable results as `OperationSummary` and produces concrete instances for lowering. | Sema effect/termination fixed points compose with Q02 instances rather than infer a second call type. |
| `docs/design/Styio-Exact-Literals-and-Builtin-Add.md` | Q05 owns exact literals, checked integer overflow, and strict IEEE Add rows while deferring order/stop/publication to Q03-F. | Optimizer and constant-evaluation tests must preserve both owners simultaneously. |

## Current compiler inventory

### Missing unified facts

| Surface | Current evidence | Gap |
|---|---|---|
| IR effect classification | `src/StyioIR/EffectKind.hpp` defines a standalone enum whose `Pure` comment implies folding, CSE, and elimination. Repository search finds no consumer outside that header. | One coarse enum is neither integrated nor sufficient for access regions, completion, totality, identity, or the four distinct rights. It must not become a parallel authority. |
| Optimizer speculation | `src/StyioLowering/StyioIROptimizer.cpp` implements `is_speculatable_op` / `ir_expr_is_speculatable` by operation and node kind, and contains a note to enable a framework when effect queries are complete. | Heuristics currently bypass canonical Sema facts, completion sets, normal return, allocation/identity, and resource versions. |
| Sema/AST summary | `src/StyioSema/TypeInfer.cpp`, `src/StyioAST`, and `src/StyioIR` have no common `EvaluationFacts` publication/consumption path. | The compiler cannot prove safe-pure siblings or diagnose all unordered order-sensitive siblings from one record. |
| Resource topology | `src/StyioResourceTopology/ResourceTopology.cpp` records ownership, flow, commit, mutation, backpressure, borrow, and explicit `HappensBefore`; it performs cycle and unordered-exclusive validation. | Useful capability/access facts exist, but the graph is not the general expression scheduler and its repeated edge scans are not the desired resource-version data structure. |

### Exact-once and traversal gaps

| Surface | Current evidence | Gap/owner |
|---|---|---|
| Arrow parsing | `src/StyioParser/NewParserExpr.cpp` constructs `FlowBindAST` for a bare name after `->` and otherwise calls `parse_resource_redirect_tail_latest`; `src/StyioParser/Parser.cpp` builds `ResourceRedirectAST` for resource-shaped targets. | This is a Q01-owned canonicalization blocker. Q03 records it only to gate its integration; it does not patch the parser or build a compatibility node. |
| Flow/await representation | `src/StyioAST/AST.hpp` contains `FlowBindAST::Create`, `CreateAwait`, pull/await/declare-target/fallback state; `src/StyioIR/GenIR/SIOIR.hpp` contains `SIOFlowBind`. | Q01 exclusively owns deletion of these direction forks. Their presence means the Q03 directional integration checkpoint waits. |
| Resource redirect | `src/StyioAST/AST.hpp` has `ResourceRedirectAST`; parser, Sema, topology, lowering, visitors, cloning, repr, IR/backend paths distinguish it from flow bind. | Q01 owns the arrow/non-arrow classification and arrow-path deletion. Q03 consumes only the resulting canonical directional node. |
| Resource-method inlining | `clone_resource_method_body_latest` in `src/StyioLowering/AstToStyioIR.cpp` maps parameter names directly to argument ASTs and clones the method body; call/property lowering invokes that clone. | Repeated parameter occurrences can clone/re-evaluate receiver or argument expressions. ENF must bind each prerequisite to one value before inlining. |
| C++ lowering order | Multiple lowering calls construct parent IR directly from child `toStyioIR(...)` calls; the current code does not expose one named value-definition phase for all strict inputs. | C++ evaluation/traversal order must not define language order. ENF and DAG construction must be explicit. |
| Block lowering | `AstToStyioIRLowerer::toStyioIR(MainBlockAST*)` iterates source statements and appends IR sequentially after resource-topology validation. | Sequential vector construction alone cannot distinguish movable safe-pure work from completion/effect/commit/publication edges. |

### Lazy/control gaps

| Surface | Current evidence | Gap/deletion target |
|---|---|---|
| Fallback | `StyioToLLVM::toLLVMIR(SGFallback*)` evaluates `primary` and `alternate` before testing/merging. | The alternate is eager and may execute effects/completions even when unselected. |
| Wave merge | `StyioToLLVM::toLLVMIR(SGWaveMerge*)` evaluates the condition, true value, and false value before LLVM `select`. | Source-defined conditional selection is eager. It must lower to branches and a typed merge. |
| Guard select | `StyioToLLVM::toLLVMIR(SGGuardSelect*)` evaluates base and guard before `select`. | The representation does not prove pattern/guard/body control dependencies or selected-only execution. |
| Settlement lowering order | `AstToStyioIRLowerer::toStyioIR(ResourceEffectAST*)` lowers operation, fallback, then all handler bodies into one structural node; the node carries string names/discard/value-required state. | The IR shape does not make recovery regions lazy, typed by resolved family identity, or protected by explicit completion successors. |
| Flow fallback | `AstToStyioIRLowerer::toStyioIR(FlowBindAST*)` lowers fallback before source and applies undefined/source-type repairs. | This demonstrates why Q03 cannot adapt legacy flow semantics. Q01 must remove the path before Q03 directional/lazy integration; Q03 does not re-own it. |

### Ambient error-state inventory

| Surface | Current evidence | Gap/deletion target |
|---|---|---|
| State owner | `src/StyioRuntime/RuntimeState.cpp` stores thread-local error flag, message, and subcode; `RuntimeState.hpp` exposes clear/set/has/last/match/take APIs and `RuntimeErrorSnapshot`. | Completion is ambient state rather than an explicit producer result and CFG edge. |
| Native helper producers | `src/StyioExtern/ExternLib.cpp` forwards `set_runtime_error_once` and many file/task/parse/container/matrix failures into that state. | All producer domains must migrate before the state can be deleted. |
| C ABI | `src/StyioExtern/ExternLib.cpp` and `.hpp` export `styio_runtime_has_error`, `last_error`, `last_error_subcode`, `error_matches_effect`, and `clear_error`. | These are compatibility ABI, not Q03-F outcomes. |
| Codegen consumers | `src/StyioCodeGen/CodeGenG.cpp` and `CodeGenIO.cpp` contain runtime-error guard emission and settlement/flow logic that queries ambient state. | Generated CFG must branch directly on typed outcomes. |
| JIT | `src/StyioJIT/StyioJIT_ORC.hpp` registers the ambient error symbols. | Explicit-outcome code needs no global error symbols. |
| Driver/native wrapper | `src/main.cpp` clears, polls, reports, and converts the ambient state into execution status in both JIT and generated native wrapper paths. | The entry point must return one explicit top-level outcome. |

## Test and gate evidence to discover during execution

The repository has feature suites under `tests/features`, parser/IDE fuzz corpora under `tests/fuzz`, CTest registrations, resource/topology tests, execution/security checks, and runbooks under `docs/teams`. Q01 owns classification/deletion of legacy arrow/task-await syntax fixtures. Q03 owns canonical directional ordering fixtures plus resource-effect, file/stdio, control-flow, scalar, function, state-resource, generated/native-outcome, and optimizer fixtures, which must be classified as:

1. retained semantic coverage migrated to canonical Q03-F;
2. new expected-red conflict/verifier coverage;
3. obsolete compatibility coverage deleted with its implementation; or
4. adjacent-owner coverage left unchanged.

The final structural audit must search source, generated artifacts, tests, docs, and gates. Removing a class while leaving a positive fixture or CI assertion that requires it does not satisfy migration.

## Comparable compiler practice

These sources guide implementation data structures; Styio semantics remain defined by its accepted owner.

| Primary source | Relevant practice | Styio use and limit |
|---|---|---|
| [MLIR Side Effects & Speculation](https://mlir.llvm.org/docs/Rationale/SideEffectsAndSpeculation/) | Separates memory/resource effects from conditional speculatability, includes allocation/free and nontermination, and models abstract resource hierarchy. | Supports separate access/effect and speculation facts. Styio additionally carries finite completions, Block stop/publication, identity observability, and four rights. |
| [LLVM MemorySSA](https://llvm.org/docs/MemorySSA.html) | Uses versioned `MemoryDef`/`MemoryUse`/`MemoryPhi` relations to avoid common quadratic dependence analyses and warns that volatile/atomic legality needs more than memory versions. | Motivates per-region version/def-use structures and fail-closed barriers, not copying LLVM's single-memory variable or treating versions as complete optimization authority. |
| [LLVM Language Reference](https://llvm.org/docs/LangRef.html) | Defines strong, distinct contracts for memory effects, `willreturn`, `noreturn`, and optimization attributes. | Backend attributes must be derived only after Q03-F facts prove the corresponding contract. LLVM UB or attribute inference cannot define Styio totality/completion semantics. |
| Q03-F historical sources in the semantic owner | Compare eager dependency models, left-to-right state timelines, unspecified effectful order, and effect-set versus sequencing distinctions. | They justify the accepted language decision but do not import thunks, monads, handlers, automatic parallelism, or exception runtimes. |

## Derived implementation decisions

1. Keep `OperationSummary` untouched and place all Q03-F fields in a new fact domain.
2. Treat rights as derived queries over primitive facts, never writable optimizer annotations.
3. Normalize once-only values before DAG construction; do not attempt to repair duplicated AST lowering after code generation.
4. Build dependencies incrementally with region/version state and adjacency lists; do not answer each access by scanning every prior edge.
5. Represent lazy behavior and completion stop as CFG before LLVM instruction selection; LLVM `select` cannot restore laziness after operands execute.
6. Treat this Q03-F lifecycle as the sole owner of the explicit Outcome ABI and ambient-state deletion: freeze the ABI before migrating producers, then delete global/TLS state, JIT symbols, and CLI/native polling only after all three domains and all consumers converge.
7. Activate optimizer transformations only after the verifier can prove facts/edges survive lowering and global error side channels are absent.

## Evidence gaps that block implementation completion

Before any implementation checkpoint can complete, its owner must refresh this inventory against the then-current source revision, including concurrent Q01/Q02/Q05 work. Missing generated/native producer lists, native/FFI declarations, resource aliases, task outcome layouts, or test registrations are preparation drift, not permission to preserve a fallback. Newly discovered adjacent policy is routed to its owner and does not expand Q03-F.
