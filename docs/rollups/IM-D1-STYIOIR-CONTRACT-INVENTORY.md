# IM-D1 StyioIR Contract Inventory

**Purpose:** Record the implementation inventory for IM-D1 so StyioIR contract work is judged by explicit lowering behavior instead of scattered placeholder returns.

**Last updated:** 2026-05-20

**Status:** Active contract inventory. This document supports [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md) §5.7 `IM-D1`.

## Contract Manifest

| Capability | Current state | Evidence |
|------------|---------------|----------|
| Active IR marker | Every concrete `StyioIR` node inherits `is_active() == true` unless a future tombstone/compatibility node overrides it | [../../src/StyioIR/StyioIR.hpp](../../src/StyioIR/StyioIR.hpp) |
| Explicit no-op | Intentional empty source forms lower to `SGNoOp`, not integer zero | [../../src/StyioIR/GenIR/SGIR.hpp](../../src/StyioIR/GenIR/SGIR.hpp), [../../src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) |
| Independent verifier | Lowering feeds an independent StyioIR verifier before codegen; resource/handle contract state is verifier-owned side-table state | [../../src/StyioIR/Verifier.cpp](../../src/StyioIR/Verifier.cpp) |
| Codegen gate | `SGMainEntry`, `SGEntry`, and `SGBlock` codegen require verified active StyioIR before LLVM emission | [../../src/StyioCodeGen/CodeGenG.cpp](../../src/StyioCodeGen/CodeGenG.cpp) |
| Placeholder retirement | Direct unsupported AST lowering now raises `StyioTypeError` instead of silently producing `SGConstInt(0)` | [../../src/StyioLowering/AstToStyioIR.cpp](../../src/StyioLowering/AstToStyioIR.cpp) |
| Regression coverage | Contract tests cover active defaults, no-op lowering, char lowering, fail-closed unsupported AST nodes, inactive verifier rejection, and codegen rejection | [../../tests/security/styio_security_test.cpp](../../tests/security/styio_security_test.cpp) |

## Implemented In This Contract Slice

1. `CommentAST`, `EmptyAST`, `PassAST`, `EOFAST`, accepted `ResourceAST` prelude metadata, accepted `ResourceMethodDefAST` / `ResourceOrderAST` topology metadata, standard-stream alias binding, and standard-stream handle aliases use explicit `SGNoOp`.
2. `CharAST` now lowers to `SGConstChar`, reports `char` / 8-bit AST type, and maps to LLVM `i8`.
3. Direct lowering for unsupported runtime values or metadata nodes fails closed with `StyioTypeError`.
4. Resource method/property clone paths reject missing inlined bodies instead of inventing a placeholder value.
5. Unsupported resource method calls and unsupported resource property accesses fail closed.
6. Function-level match sugar (`# f(x) ?= { ... }`) lowers `CasesAST` through the first parameter as an explicit `SGMatch` instead of relying on the former integer placeholder.

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
| `TypeTupleAST`, `OptArgAST`, `OptKwArgAST`, `VarTupleAST` | Declaration metadata | Fails closed when lowered directly; parent declarations must consume them. |
| `TypeConvertAST` | Implementation debt | Fails closed until value-carrying cast IR exists. |
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
| `BoolAST`, `IntAST`, `FloatAST`, `CharAST`, `StringAST`, `TypeAST` | Leaf type carrier | Their type comes from the node or type token. Keep inference side-effect free. |
| `VarAST`, `ParamAST`, `OptArgAST`, `OptKwArgAST`, `TypeTupleAST`, `VarTupleAST` | Declaration metadata | Parent declarations must validate and bind them. Direct runtime lowering is rejected. |
| `NoneAST`, `TypeConvertAST`, `InfiniteAST`, `TupleAST`, `ExtractorAST`, `SetAST`, `AnonyFuncAST` | Implementation debt | Add real sema and IR only when the language semantics are accepted; otherwise keep typed rejection. |
| `StructAST`, `RangeAST`, `EmptyResourceAST`, `ResPathAST`, `RemotePathAST`, `WebUrlAST`, `DBUrlAST`, `ExtPackAST`, `ReadFileAST` | Partially retired or metadata-heavy syntax | Keep fail-closed or parent-consumed behavior until the owning design SSOT accepts runtime semantics. |
| `ResourceAST`, `ResourceMethodDefAST`, `ResourceOrderAST` | Accepted resource/topology metadata | Lowering is explicit `SGNoOp`; collection and validation happen before executable lowering without creating runtime placeholder values. |
| `ExportDeclAST`, `ExternBlockAST` | Contract metadata | Sema may remain side-effect free while native interop ownership is collected elsewhere. |
| `BreakAST`, `ContinueAST`, `ReturnAST` | Control-flow syntax | Current local behavior is accepted; a future control-flow verifier can add dominance/reachability checks. |
| `ForwardAST`, `BackwardAST`, `CODPAST`, `CheckEqualAST`, `CheckIsinAST`, `HashTagNameAST` | Retired or parser-metadata syntax | Keep fail-closed unless a future design reactivates them. |
| `CasesAST`, `MatchCasesAST`, `StateRefAST` | Parent/context-sensitive syntax | Continue tightening sema in the owning match/pulse checkpoints. |

## IM-D1 Closure Position

The StyioIR contract part of IM-D1 is implemented: no active direct AST lowering path should silently use `SGConstInt(0)` as a placeholder, codegen requires verified active IR, and intentional no-op source forms use explicit `SGNoOp`.

Remaining work in this area is no longer an implicit IM-D1 decision. It is feature implementation debt: tuple/set values, null/unit semantics, value-carrying casts, closures, retired flow syntax, and resource/path value semantics need separate accepted-language decisions before they can become runnable.
