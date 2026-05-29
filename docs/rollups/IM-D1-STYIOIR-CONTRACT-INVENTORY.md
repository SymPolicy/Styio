# IM-D1 StyioIR Contract Inventory

**Purpose:** Record the implementation inventory for IM-D1 so StyioIR contract work is judged by explicit lowering behavior instead of scattered placeholder returns.

**Last updated:** 2026-05-30

**Status:** Active contract inventory. This document supports [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md) §5.7 `IM-D1`.

## Contract Manifest

| Capability | Current state | Evidence |
|------------|---------------|----------|
| Active IR marker | Every concrete `StyioIR` node inherits `is_active() == true` unless a future tombstone/compatibility node overrides it | [../../src/StyioIR/StyioIR.hpp](../../src/StyioIR/StyioIR.hpp) |
| Explicit no-op | Intentional empty source forms lower to `SGNoOp`, not integer zero | [../../src/StyioIR/GenIR/SGIR.hpp](../../src/StyioIR/GenIR/SGIR.hpp), [../../src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) |
| Independent verifier | Lowering feeds an independent StyioIR verifier before codegen; resource/handle contract state is verifier-owned side-table state | [../../src/StyioIR/Verifier.cpp](../../src/StyioIR/Verifier.cpp) |
| Codegen gate | `SGMainEntry`, `SGEntry`, and `SGBlock` codegen require verified active StyioIR before LLVM emission | [../../src/StyioCodeGen/CodeGenG.cpp](../../src/StyioCodeGen/CodeGenG.cpp) |
| Placeholder retirement | Direct unsupported AST lowering now raises `StyioTypeError` instead of silently producing `SGConstInt(0)` | [../../src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) |
| Value-carrying scalar casts | `TypeConvertAST` lowers to `SGCast(value, from_type, to_type)` for compiler-owned scalar promotions instead of failing closed or returning a placeholder | [../../src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp), [../../src/StyioCodeGen/CodeGenG.cpp](../../src/StyioCodeGen/CodeGenG.cpp) |
| Regression coverage | Contract tests cover active defaults, no-op lowering, char lowering, match `i64`/`f64`/`bool`/`char`/`string` result-family lowering, resource-method char, format-string, list/list-op, and dict inline cloning, fail-closed unsupported AST nodes, inactive verifier rejection, and codegen rejection | [../../tests/security/styio_security_test.cpp](../../tests/security/styio_security_test.cpp), [../../tests/styio_test.cpp](../../tests/styio_test.cpp) |

## Implemented In This Contract Slice

1. `CommentAST`, `EmptyAST`, `PassAST`, `EOFAST`, accepted `ResourceAST` prelude metadata, accepted `ResourceMethodDefAST` / `ResourceOrderAST` topology metadata, standard-stream alias binding, and standard-stream handle aliases use explicit `SGNoOp`.
2. `CharAST` now lowers to `SGConstChar`, reports `char` / 8-bit AST type, and maps to LLVM `i8`.
3. Direct lowering for unsupported runtime values or metadata nodes fails closed with `StyioTypeError`.
4. Resource method/property clone paths reject missing inlined bodies instead of inventing a placeholder value.
5. Unsupported resource method calls and unsupported resource property accesses fail closed.
6. Function-level match sugar (`# f(x) ?= { ... }`) lowers `CasesAST` through the first parameter as an explicit `SGMatch` instead of relying on the former integer placeholder.
7. `TypeConvertAST` now carries its source value through Sema, StyioIR, verifier traversal, optimizer traversal, textual repr, and LLVM scalar conversion for `Bool_To_Int` and `Int_To_Float`.
8. `RangeAST` now validates integer expression operands in Sema, lowers constant ranges to `SCListLiteral`, and lowers expression-bound ranges to an internal list-producing `SGCall` that codegen expands into a runtime `list[i64]` fill loop.
9. Function return annotations that parse as `TypeTupleAST` now fail closed in Sema/lowering instead of using the old `i64` fallback. Tuple value returns remain open until tuple value IR exists.
10. Accepted `MatchCasesAST` / function match sugar now run semantic inference over the scrutinee, integer case patterns, arm/default bodies, and `i64`/`f64`/`bool`/`char`/`string` tail result kinds before lowering. `SGMatch` preserves `f64`, `bool`, and `char` merge widths instead of collapsing them to `i64`; mixed `bool`/`char` branch values still intentionally promote to `i64`, and container result families fail closed until value IR exists. Branch-local bindings are isolated per arm, function-body inference uses a recursion guard, recursive match functions can reuse earlier base-arm result evidence, and undefined match tail values fail closed instead of reaching codegen as default `i64` values.
11. `ListOpAST` slice selectors over materialized `list[T]` now lower to explicit `SCListSlice` IR instead of reusing index IR or parser-only acceptance. Verifier, optimizer, textual repr, LLVM codegen, ORC registration, and runtime settlement all traverse the new IR node.
12. Accepted `CharAST`, `FmtStrAST`, `ListAST`/`ListOpAST`, and `DictAST` values now also survive resource-method body parsing and state/resource-method inline cloning. User-defined resource methods such as `@file::marker = () => { >_('x') }`, `@file::summary = () => { $"value={1 + 2}" -> @stdout }`, and single-return container bounds methods such as `@file::missing = () => { <| dict{"a": 1}["missing"] }` execute or recover after inlining, while invalid multi-byte char literals, malformed format strings, and returned dict-slice shapes fail closed before broader semantics are implied.

## Remaining `SGConstInt(0)` Uses

| Location | Classification | Rationale |
|----------|----------------|-----------|
| `lower_tail_stmt(..., nullptr)` | Generated default value | This is not an AST lowering placeholder; it is the current implicit default for a missing function tail. A future return-contract checkpoint may replace it with `unit` or a required-return diagnostic. |
| `zero_value_for_type_latest(...)` integer/default branch | Real storage initializer | Resource declarations need concrete initial storage values. Integer-like storage initializes to zero intentionally. |

## Fail-Closed AST Families

These nodes are no longer allowed to pass through lowering as integer zero:

| AST family | Classification | Current behavior |
|------------|----------------|------------------|
| `NoneAST` | Implementation debt | Fails closed until null/unit/option semantics are defined. |
| `TypeTupleAST`, `OptArgAST`, `OptKwArgAST`, `VarTupleAST` | Declaration metadata | Fails closed when lowered directly; function parents now reject tuple return annotations until tuple value IR exists. |
| `TypeConvertAST` | Accepted compiler-owned scalar promotion | Lowers to value-carrying `SGCast` for `Bool_To_Int` and `Int_To_Float`; other user-facing cast syntax still requires a separate language decision. |
| `InfiniteAST` | Retired/undefined sequence syntax | Fails closed. |
| `TupleAST`, `ExtractorAST`, `SetAST` | Implementation debt | Fails closed until tuple/set value IR is implemented. |
| `StdStreamAST` | Parent-consumed resource syntax | Fails closed when lowered directly; parent resource operations consume it. |
| `EmptyResourceAST` | Resource sentinel syntax | Fails closed when lowered directly; parent redirect/release operations consume it. |
| `ResPathAST`, `RemotePathAST`, `WebUrlAST`, `DBUrlAST`, `ExtPackAST` | External resource/package metadata | Fails closed until runtime value semantics are defined. |
| `ReadFileAST` | Retired syntax | Fails closed in favor of file resources. |
| `ForwardAST`, `BackwardAST`, `CODPAST`, `CheckEqualAST`, `CheckIsinAST`, `HashTagNameAST` | Retired or parser-metadata flow syntax | Fails closed outside the owning high-level construct. |
| `AnonyFuncAST` | Implementation debt | Fails closed until closure/function-value IR is implemented. |
| `CasesAST`, `StateDeclAST` | Parent-consumed syntax | Fails closed when lowered directly; `MatchCasesAST`, function-level match sugar, and pulse topology planning own valid lowering paths. |

## Empty Sema Visitor Classification

| Visitor family | Classification | Required follow-up |
|----------------|----------------|--------------------|
| `CommentAST`, `EmptyAST`, `PassAST`, `EOFAST` | Intentional no-op | No extra sema action unless the no-op contract changes. |
| `BoolAST`, `IntAST`, `FloatAST`, `CharAST`, `StringAST`, `TypeAST` | Leaf type carrier | Their type comes from the node or type token. Keep inference side-effect free. Accepted leaf literals that can appear in resource/state helper bodies must also have inline-clone coverage; `CharAST` is now covered for resource-method bodies, and `FmtStrAST` has matching resource-method inline-clone evidence even though its embedded expressions own their inference. |
| `VarAST`, `ParamAST`, `OptArgAST`, `OptKwArgAST`, `TypeTupleAST`, `VarTupleAST` | Declaration metadata | Parent declarations must validate and bind them. Function return annotations reject `TypeTupleAST` until tuple value IR exists; direct runtime lowering is rejected. |
| `NoneAST`, `InfiniteAST`, `TupleAST`, `ExtractorAST`, `SetAST`, `AnonyFuncAST` | Implementation debt | Add real sema and IR only when the language semantics are accepted; otherwise keep typed rejection. |
| `TypeConvertAST` | Accepted compiler-owned scalar promotion | Keep the value-carrying `SGCast` path limited to internally selected scalar promotions until source-level cast syntax is accepted. |
| `RangeAST` | Accepted collection syntax | Validate integer `start`, `end`, and optional `step`; materialize `list[i64]` for expression/value use and keep iterator lowering on the dedicated range loop path. |
| `StructAST`, `EmptyResourceAST`, `ResPathAST`, `RemotePathAST`, `WebUrlAST`, `DBUrlAST`, `ExtPackAST`, `ReadFileAST` | Partially retired or metadata-heavy syntax | Keep fail-closed or parent-consumed behavior until the owning design SSOT accepts runtime semantics. |
| `ResourceAST`, `ResourceMethodDefAST`, `ResourceOrderAST` | Accepted resource/topology metadata | Lowering is explicit `SGNoOp`; collection and validation happen before executable lowering without creating runtime placeholder values. |
| `ExportDeclAST`, `ExternBlockAST` | Contract metadata | Sema may remain side-effect free while native interop ownership is collected elsewhere. |
| `BreakAST`, `ContinueAST`, `ReturnAST` | Control-flow syntax | `ReturnAST` now infers its expression for accepted function/task/match contexts; a future control-flow verifier can add dominance/reachability checks. |
| `ForwardAST`, `BackwardAST`, `CODPAST`, `CheckEqualAST`, `CheckIsinAST`, `HashTagNameAST` | Retired or parser-metadata syntax | Keep fail-closed unless a future design reactivates them. |
| `CasesAST`, `MatchCasesAST` | Accepted parent/context-sensitive match syntax | Match parents now infer scrutinee, integer patterns, arm/default bodies, branch-local scopes, and `i64`/`f64`/`bool`/`char`/`string` tail result kinds; direct `CasesAST` lowering remains parent-consumed. |
| `StateRefAST` | Parent/context-sensitive pulse syntax | Continue tightening sema in the owning pulse checkpoints. |

## IM-D1 Closure Position

The StyioIR contract part of IM-D1 is implemented: no active direct AST lowering path should silently use `SGConstInt(0)` as a placeholder, codegen requires verified active IR, and intentional no-op source forms use explicit `SGNoOp`.

Remaining work in this area is no longer an implicit IM-D1 decision. It is feature implementation debt: tuple/set values, null/unit semantics, source-level cast syntax beyond compiler-owned scalar promotion, closures, broader match result families beyond the current `i64`/`f64`/`bool`/`char`/`string` lowering path, retired flow syntax, resource/path value semantics, and the still-incomplete state inline clone surface beyond the covered `CharAST` and `FmtStrAST` resource-method paths need separate accepted-language decisions or focused implementation slices before they can become runnable.
