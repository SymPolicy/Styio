# Q05-LIT-ADD Evidence

**Purpose:** Ground the Q05-LIT-ADD implementation plan in current repository paths, accepted owner boundaries, and primary compiler evidence.

**Last updated:** 2026-07-20

**Plan ID:** `a0063a94-8e76-4121-937a-0a43fa94b8d1`

## Authority and method

The accepted [Q05 exact literals and built-in Add owner](../../design/Styio-Exact-Literals-and-Builtin-Add.md) supplies language semantics. The [Q02 principal-inference plan](../Styio-Callable-Principal-Inference-Plan.md) supplies the integration boundary, while the operation-completion and directional-flow owners retain generic completion handling. Evidence below explains why a one-shot implementation migration is required; current code behavior is not normative.

Repository inspection covered parser/token paths, AST storage, Sema `Add`, constant folding, SGIR, LLVM type/emission paths, IDE publication, tests, and build registration. External evidence is limited to primary LLVM/Clang documentation and source.

## Current repository facts

| Area | Current fact | Required implication |
|---|---|---|
| Tokenization | `src/StyioParser/Tokenizer.cpp` scans numeric digits and the optional decimal fraction without numeric token/digit/work gates. | Add a cheap preflight budget before arbitrary-precision construction, with exact at-limit and one-over tests. |
| AST/parser | `Parser.cpp` and `NewParserExpr.cpp` copy numeric spelling into `IntAST`/`FloatAST`; negative source forms prepend a sign. `IntAST` carries a string and weak bit metadata, while `FloatAST` immediately presents `f64`. | Preserve lexeme for source fidelity, but publish one canonical semantic `ExactLiteral` side-table fact; remove early `f64` authority. |
| Sema | `TypeInfer.cpp` binary inference routes string concatenation, numeric/string coercion, AST-kind branching, `getMaxType` promotion, and fallback numeric-family selection through one large path. Compound assignment can default unknown numeric state to `i64`. | Replace scalar `Add` selection atomically with the closed catalog; reject mixed concrete and excluded families. Do not infer the new table from the old matrix. |
| Promotion helper | `src/StyioToken/Token.cpp` owns `getMaxType`, and existing tests encode widening behavior. | Remove its authority from `Add`; retain it only where an independently owned non-`Add` rule still requires it. Rewrite obsolete tests rather than preserving compatibility. |
| Lowering | `AstToStyioIR.cpp` converts literal strings through `std::stoll` on multiple paths. | Lower from checked semantic facts and concrete row identity, never host-width conversion. |
| Constant folding | `StyioIROptimizer.cpp` parses with `std::stol`/`std::stod`, computes through host `long`/`double`, and serializes with `std::to_string`. | Replace with APInt/APFloat-backed evaluation sharing materialization and row descriptors; host precision and undefined signed overflow cannot participate. |
| SGIR | `SGConstInt` defaults to 64 bits, `SGConstFloat` lacks an explicit format, and generic `SGBinOp` can carry undefined operand state. | Encode concrete width/format, selected operation row, and finite completion facts; verify before backend entry. |
| LLVM type mapping | `src/StyioCodeGen/GetTypeG.cpp` maps language integers to LLVM `i64`, floats to `double`, and contains default `i64` repairs. | Map all admitted widths and formats exactly and reject unresolved state. |
| LLVM emission | `CodeGenG.cpp` reparses literals with host functions, converts string pointers to numbers, performs implicit int/float conversions, guesses `Add` flavor, and emits unchecked `CreateAdd` or permissive `FAdd`. | Emit only the selected concrete row: checked integer intrinsic plus completion branch, or strict constrained floating add. |
| Tests | `tests/styio_test.cpp`, `tests/typeinfer_internal_test.cpp`, `tests/lowering_internal_test.cpp`, and `tests/security/styio_security_test.cpp` contain promotion, string-plus-number, matrix, folding, and generic `Add` assumptions. | Classify each fixture as obsolete scalar behavior, separately owned adjacent behavior, or reusable infrastructure; delete or rewrite only with explicit ownership. |
| Tooling | `CompilerBridge.cpp` exposes present AST/Sema concrete types and mostly generic type failures; IDE tests cover hover, diagnostics, caches, and performance. | Publish selected literal/row facts and stable diagnostic codes through the existing bridge, with CLI/IDE and cold/warm cache equivalence. |

## External implementation evidence

- LLVM's [APInt](https://llvm.org/doxygen/classllvm_1_1APInt.html) and [APSInt](https://llvm.org/doxygen/classllvm_1_1APSInt.html) provide arbitrary-width integer storage and signed interpretation suitable for exact integers and decimal coefficients. Their size-aware operations avoid host integer authority.
- LLVM's [APFloat](https://llvm.org/doxygen/classllvm_1_1APFloat.html) exposes explicit IEEE semantics, `convertFromString`, `convertFromAPInt`, arithmetic rounding modes including nearest-ties-to-even, and operation status. These interfaces support deterministic materialization and constant evaluation without host `double` parsing.
- Clang's [NumericLiteralParser](https://clang.llvm.org/doxygen/classclang_1_1NumericLiteralParser.html), [LiteralSupport declarations](https://clang.llvm.org/doxygen/LiteralSupport_8h_source.html), and [implementation](https://clang.llvm.org/doxygen/LiteralSupport_8cpp_source.html) demonstrate the separation between token spelling and APInt/APFloat conversion with explicit overflow/rounding handling.
- Clang's [constant evaluator](https://clang.llvm.org/doxygen/ExprConstant_8cpp.html) tracks APFloat rounding and operation status rather than delegating semantics to host arithmetic.
- The LLVM [Language Reference](https://llvm.org/docs/LangRef.html) defines `llvm.sadd.with.overflow.*`, constrained floating operations, `strictfp`, denormal modes, and fast-math flags. Checked add returns a sum/overflow pair suitable for a direct success/completion branch; strict floating emission must exclude flags such as `reassoc`, `nsz`, `nnan`, and `ninf`.
- LLVM's [IRBuilder API](https://llvm.org/doxygen/classllvm_1_1IRBuilder.html) supplies the construction seam, but semantic row selection must happen before that backend boundary.

## Chosen implementation evidence direction

1. APInt/APSInt are the single exact integer/coefficient authority. Decimal exactness is coefficient plus checked base-ten exponent and negative-zero metadata; no runtime decimal type is introduced.
2. APFloat with explicit target semantics and `rmNearestTiesToEven` is the compile-time materialization/arithmetic authority. Conversion status is inspected; finite decimal-to-infinity fails, while underflow/subnormal results remain admitted.
3. LLVM `sadd.with.overflow` is the signed runtime primitive. Constrained floating add, strict function/target attributes, IEEE denormal mode, and absence of fast-math flags form one auditable backend contract.
4. Deterministic budgets are measured counters, not timeouts. A fixed checked-in corpus must justify finite defaults before enablement; semantic tests bind each chosen value at `N` and `N+1`.
5. The scalar relation is a small immutable indexed table with stable row IDs. Q02 consumes it by interface and fingerprint; Q05 does not own generalization state or specialization caches.

## Gaps and risks the checkpoints must close

- LLVM version/target combinations may differ in constrained-FP and denormal support. The backend checkpoint must enumerate supported triples and fail closed or select a verified repository-owned helper; ordinary `fadd` is not a silent fallback.
- APFloat decimal conversion status and negative-zero construction need focused bit-pattern tests. `opInexact` is expected for admitted decimal rounding, while overflow-to-infinity is not.
- Exact decimal alignment and literal-literal folding can cause large powers or limb expansion even for short metadata. Work accounting must precharge using checked operand-size formulas before allocation.
- Q02's existing plan mentions a Q05 relation integration node. Execution owners must narrow it to consumption of this plan's sole catalog rather than creating a second catalog; this plan does not edit Q02-owned files during planning.
- Existing matrix/string fixtures mix unauthorized scalar `Add` with adjacent features. A fixture inventory and owner-aware removal receipt is required so convergence neither preserves accidental rows nor erases unrelated capabilities.
- There is no accepted performance baseline or limit configuration yet. The implementation may not enable the new path until the evidence node records corpus, toolchain, thresholds, peak-memory method, and reproducible results.

## Evidence completion rule

Planning evidence establishes the migration need and feasible primitives. Execution evidence must add the selected finite limit values, target capability matrix, catalog fingerprint, benchmark/memory baselines, deleted-path inventory, and exact test/IR receipts on the final revision. No current implementation behavior can override the accepted owner.
