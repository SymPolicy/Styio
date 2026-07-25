# Callable Principal Inference Evidence

**Purpose:** Ground Q02-INF delivery in current repository behavior, accepted owner boundaries, and primary compiler/language practice.

**Last updated:** 2026-07-20

## Semantic evidence and decision prerequisites

| Evidence | What it establishes | Planning consequence |
|---|---|---|
| [Styio-Callable-Principal-Inference.md](../../design/Styio-Callable-Principal-Inference.md) | Landed sole owner of the approved Q02-INF semantics. | Every implementation and validation decision traces to this authority; the plan does not maintain a competing policy. |
| [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md), Q02-INF discussion | Historical decision evidence for the two required examples, rejection of first-use/whole-call-graph inference, and routing of exact `Add`/literal behavior to Q05. | The review explains provenance, while the dedicated semantic owner remains authoritative. |
| [Styio-Language-Decision-Ledger.md](../../design/Styio-Language-Decision-Ledger.md) | Tracks which callable, numeric-default, capture, generic, and export decisions are owned elsewhere. | Requirements preserve Q02/Q04/Q05/F02/Q10 boundaries instead of silently closing them. |
| Accepted Q02-SIG operation-summary contract | Explicit boundaries and finite completion upper bounds are upstream facts. | A scheme can carry one concrete finite completion summary but cannot invent completion-row polymorphism. |
| [Styio-Exact-Literals-and-Builtin-Add.md](../../design/Styio-Exact-Literals-and-Builtin-Add.md) | Accepted `Q05-LIT-ADD` owner: exact terms, fail-closed materialization, finite same-type scalar rows, late `i64`/`f64` defaults, checked integer `overflow`, strict floating rows, and conservative `{overflow}` generalized bounds. | The semantic gate is satisfied. Callable inference consumes the catalog delivered by the sibling Q05 plan; the current implementation matrix remains migration evidence, never language authority. |

## Repository evidence

### Parser and syntax are already sufficient

- `src/StyioParser/HashFunctionParser.hpp:192-380` parses `# name`, optional result annotation, `:=`/`=`, parameters, and expression-bodied `=>` callables.
- `src/StyioParser/Parser.cpp:5039-5076` accepts parameters without annotations and constructs an untyped `ParamAST`.
- The nightly route reuses the hash-function parser in `src/StyioParser/NewParserExpr.cpp:2597-2617,2855-2870`; its Pratt loop maps and constructs `+` in `src/StyioParser/NewParserExpr.cpp:1428-1445,1982-2028`.
- `tests/parser_internal_test.cpp:622-636` already covers untyped hash-function parameters in both assignment spellings.

**Conclusion:** Q02-INF needs no token, keyword, grammar production, or syntax AST node. Parser changes would create a duplicate source authority.

### AST stores syntax plus mutable concrete type slots, not schemes

- `src/StyioAST/AST.hpp:193-255` represents `TypeAST` with one concrete `StyioDataType`.
- `src/StyioAST/AST.hpp:752-901` gives `VarAST`/`ParamAST` mutable concrete type annotations and setters.
- `src/StyioAST/AST.hpp:1631-1685` stores one concrete result type in `BinOpAST`; `src/StyioAST/AST.hpp:2002-2106` gives `FuncCallAST` only callee/name/arguments, without a scheme or instance signature.
- `src/StyioAST/AST.hpp:4188-4647` represents regular and expression-bodied callables. The `FunctionAST::hasRetType`/`setRetType` helpers around `4350-4364` are inconsistent and cannot become a second semantic-store route.

**Conclusion:** Inference facts belong in Sema side tables keyed by resolved identities. Parser AST ownership should remain syntactic.

### Concrete type identity is reusable only after solving

- `src/StyioToken/Token.hpp:15-36,117-197` defines a flat concrete `StyioDataType`; it has no type-variable identity, constraint, substitution, or callable signature structure.
- `src/StyioSession/TypeTable.hpp:16-109` and `src/StyioSession/TypeTable.cpp:30-78` intern canonical concrete descriptors and built-ins. `SemaContext::intern_type` deliberately refuses `Undefined` at `src/StyioSema/SemaContext.hpp:355-380`.
- `CompilationSession` owns the table at `src/StyioSession/CompilationSession.hpp:38-54,327-333`.

**Conclusion:** `TypeTable` is the correct solved-type repository and a useful local deduplication input. Mutable inference variables require a scoped arena. Cross-session fingerprints must encode canonical type content rather than raw session-local `TypeId`/symbol ordinals.

### Current Sema is first-use mutation, not principal inference

- Function definitions and local concrete types are stored in `src/StyioSema/SemaContext.hpp:178-195,540-637`; inferred returns are one map entry per function name/SymbolId at `835-840`.
- `src/StyioSema/TypeInfer.cpp:1151-1174` treats `Undefined` parameters as accepting anything; it does not unify type variables.
- `src/StyioSema/TypeInfer.cpp:1177-1238,1812-1872` reads/writes one concrete inferred-return cache per function.
- `infer_expr_type` resolves ordinary calls through that single result at `src/StyioSema/TypeInfer.cpp:1282-1450`.
- The call checker at `src/StyioSema/TypeInfer.cpp:3527-3841` infers arguments, then at `3727-3739` executes `func_args[i]->setDataType(arg_types[i])` for an untyped parameter. Its saved maps at `3757-3800` do not restore that shared AST mutation, and body inference records one concrete result at `3802-3833`.
- Callable declarations at `src/StyioSema/TypeInfer.cpp:4001-4019` only register definitions/signature annotations; the main block pre-registers definitions and later visits statements at `4475-4571`.
- Existing positive tests `tests/styio_test.cpp:5371-5401` and `tests/typeinfer_internal_test.cpp:2274-2284` use only one concrete call, so they do not prove independent instantiation.

**Conclusion:** The first call permanently specializes shared syntax and can make later correct calls order-dependent. The old mutation and single-return caches must be deleted when the scheme/call tables become authoritative.

### `+` has concrete rules but no symbolic relation

- `src/StyioSema/TypeInfer.cpp:2981-3231` recursively obtains concrete operands, applies string/numeric cases, dispatches many syntax-node combinations, and falls back to numeric promotion at `3206-3213`.
- An unbound `x` remains `Undefined`; `x + 5` creates no reusable constraint until a call has already mutated `x` to a concrete type.

**Conclusion:** The current code is useful evidence for migration inventory and
regression discovery, but accepted `Q05-LIT-ADD` requires it to be replaced by
one authoritative finite catalog. Q02 generates `OperatorConstraint` facts and
solves them only through the Q05-owned catalog interface.

### Lowering, SGIR, and codegen assume one concrete function by source name

- `src/StyioLowering/AstToStyioIR.cpp:112-155` maps unspecified callable returns/parameters to `i64` defaults.
- Callable lowering at `src/StyioLowering/AstToStyioIR.cpp:4196-4328` emits one `SGFunc` per source definition and uses the source name. Main lowering visits each source definition once at `4793-4854`.
- Ordinary call lowering at `src/StyioLowering/AstToStyioIR.cpp:4003-4036` emits only the source function name and arguments.
- `src/StyioIR/GenIR/SGIR.hpp:59-71` makes `SGType` concrete; `SGFunc` is concrete at `412-457`, but `SGCall` contains only name/arguments at `459-476`.
- `src/StyioCodeGen/GetTypeG.cpp:69-90,215-220` maps concrete types and otherwise derives a call result from module lookup with an `i64` fallback.
- `src/StyioCodeGen/CodeGenG.cpp:2061-2083,2297-2305` declares/defines one LLVM function per name; calls at `2736-2782` coerce arguments to that one signature.

**Conclusion:** Unresolved terms cannot enter SGIR. Reachable concrete specializations require stable internal names/signatures and a worklist before LLVM emission; codegen defaults are not a compatibility strategy.

### IDE and build integration need a shared fact boundary

- `src/main.cpp:6114-6160` uses the same `AstToStyioIRLowerer` instance for Sema and lowering, so plan-local side tables can bridge the passes without AST mutation.
- `src/StyioServices/StyioIDE/CompilerBridge.cpp:380-384` currently reads concrete local bindings and function definitions from the analyzer, not canonical schemes/call instances.
- `src/cmake/StyioFrontendSources.cmake:16-26` and `src/cmake/StyioBackendSources.cmake:7` explicitly register frontend/backend sources; new responsibility-focused modules require build wiring.
- `tests/CMakeLists.txt:589-604` explicitly assembles internal parser/Sema/lowering support, so adding a source file without test-target registration can leave apparent tests unlinked.

**Conclusion:** Sema must publish an explicit immutable fact interface used by lowering and IDE, and every new module/test must be registered in production and test builds.

## Primary external compiler practice

1. The [Rust compiler type-inference guide](https://rustc-dev-guide.rust-lang.org/type-inference.html) separates an inference context and variables from interned concrete types, applies equality with diagnostic context, and defers obligations that cannot yet be solved. Styio should adopt the separation and source-origin principle without importing Rust lifetimes/subtyping.
2. Rust's [canonicalization guide](https://rustc-dev-guide.rust-lang.org/traits/canonicalization.html) replaces context-local inference variables with stable canonical variables so repeated shapes can be cached and cycles detected. Styio needs the same property for scheme and constraint fingerprints, with a smaller closed rank-1 domain.
3. Rust's [monomorphization collector](https://rustc-dev-guide.rust-lang.org/backend/monomorph.html) discovers reachable concrete instances before codegen and explicitly calls out compile-time/binary-size cost. Styio should use a deduplicating worklist plus deterministic budgets rather than lower every generic definition or discover instances during LLVM emission.
4. The [OCaml value-restriction discussion](https://ocaml.org/manual/4.13/polymorphism.html) documents the unsoundness/confusion around mutation and weak polymorphism. Styio's final/capture-safe eligibility and rejection of mutable first-use variables directly address that failure mode.
5. The [GHC ambiguity check](https://ghc.gitlab.haskell.org/ghc/doc/users_guide/exts/ambiguous_types.html) rejects constraints that leave a binding unusable instead of deferring every error to calls. Styio needs a closed-world reachability/functional-determination check at definition time, not GHC's open type-class system.
6. Haskell's report describes [principal type schemes](https://www.haskell.org/onlinereport/haskell2010/haskellch4.html#x10-650004.1.4) and [numeric literal overloading/defaulting](https://www.haskell.org/onlinereport/basic.html#sect6.4.1). The useful lesson is to retain relations; the caution is that defaulting is a separate language choice. Styio has now fixed that choice for `Q05-LIT-ADD` without importing an open type-class system.

## Evidence-derived architecture choices

| Choice | Evidence-based value | Rejected shortcut |
|---|---|---|
| Scoped inference arena plus union-find | Separates mutable solving from concrete interning and supports near-linear equality merging. | Encoding distinct variables as `Undefined` or mutating `TypeAST`. |
| Canonical scheme variables and sorted residual constraints | Makes cache keys and diagnostics independent of allocation/hash order. | Persisting raw `TypeVarId`, `TypeId`, pointer, or `unordered_map` order. |
| Sema side tables for definitions/calls | Preserves syntactic AST and lets Sema/lowering/IDE share immutable facts. | Adding inferred schemes to every AST node or retaining setter-based inference. |
| Closed relation catalog supplied by Q05 | Makes `add_five` precise, bounded, and coherent. | Treating current `+` branches as implicit spec or introducing open user instances. |
| Reachability worklist with concrete SGIR | Fits existing backend architecture and exposes code-growth accounting. | Runtime dictionaries, erased ABI, or generic `SGType`. |
| Definition and specialization gates | Prevents recursive instance growth and unbounded compile memory/code size. | Timeouts, first-use cutoffs, or silent generic fallback. |

## Gaps and prerequisites to close before implementation

1. Record the landed revision of the approved `docs/design/Styio-Callable-Principal-Inference.md`; if transcription drift is found, update plan requirements and validation to match the semantic owner before code.
2. Record the exact accepted Q05 owner revision and the sibling plan's concrete
   catalog interface before starting callable-operator integration. `add_five`
   is not license to clone the rows into Q02 or to start before that shared
   interface is reviewable.
3. Confirm the Q02-SIG operation-summary implementation interface or add a narrowly owned scaffold prerequisite; do not duplicate completion analysis in the inference core.
4. Inventory all callable definition/call routes, nested/source-reachable type constructors, recursion classification facts, export/protocol/native boundaries, and capture facts before setting generalization eligibility.
5. Establish evidence-backed specialization/constraint budgets and a stable definition fingerprint source; neither raw `SymbolId` nor an unspecified hash is sufficient for persisted/cache-visible identity.
6. Confirm test source registration so new Sema/lowering/codegen unit tests execute in CI rather than merely existing on disk.

## Migration evidence to record

The evidence checkpoint updates this file with the accepted Q02/Q05 source revisions, symbol-level inventory, test-target mapping, deterministic key inputs, chosen budget measurements, and any contradicted assumption. It distinguishes repository observation from inference and routes adjacent language questions to their owner before implementation changes the target.
