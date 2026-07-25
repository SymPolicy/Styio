# Q05-LIT-ADD Architecture

**Purpose:** Define one bounded exact-literal, materialization, scalar-`Add`, IR, backend, tooling, and cache architecture for Q05-LIT-ADD.

**Last updated:** 2026-07-20

**Plan ID:** `a0063a94-8e76-4121-937a-0a43fa94b8d1`

## Architectural boundary

This architecture implements only the accepted [Q05 owner](../../design/Styio-Exact-Literals-and-Builtin-Add.md). Q05 owns exact numeric facts, materialization/default policy, the finite scalar `Add` catalog, concrete constant semantics, and each built-in row's completion contribution. Q02 owns type terms and principal inference; generic operation-summary settlement/propagation owns completion CFG mechanics. Accepted [Q03-F](../../design/Styio-Functional-Evaluation-and-Effect-Ordering.md) and its dedicated implementation plan own strict operand prerequisites, dependency/effect ordering, completion stop/publication, diagnostics, and optimizer rights; this Q05 architecture consumes those facts but does not implement them. Q06/Q08/F02 retain their active deferred policy.

```text
source lexeme
  -> lexical budget preflight
  -> canonical ExactLiteral semantic fact
  -> context/default materialization + BuiltinAddCatalog row selection
  -> NumericOperationDescriptor(success type, row, completion bound)
  -> shared constant evaluator OR verified concrete SGIR
  -> checked integer edge OR strict floating instruction
```

Only the lexeme remains syntax-owned. Every later stage consumes immutable semantic facts keyed by resolved expression identity.

## Modules and dependency direction

| Module | Responsibility | May depend on |
|---|---|---|
| `StyioSema/Numeric/ExactLiteral.*` | Exact integer/decimal values, canonical encoding, negative-zero fact | LLVM ADT/APInt only |
| `StyioSema/Numeric/NumericLiteralLimits.*` | Versioned finite limits and deterministic checked work accounting | fixed configuration primitives |
| `StyioSema/Numeric/LiteralMaterialization.*` | Failure-closed conversion/default decisions and target bit values | exact literals, APFloat, concrete scalar IDs |
| `StyioSema/Numeric/BuiltinAddRelation.*` | Immutable finite rows, stable row IDs/fingerprint, Q02 query and completion bound | materialization, concrete scalar IDs, completion family IDs |
| `StyioSema/Numeric/NumericConstantEvaluator.*` | Selected-row integer/float evaluation and completion result | operation descriptor, APInt/APFloat |
| Sema integration | Side-table publication, expected-type/default boundary calls, diagnostics | the five modules above and existing session facts |
| SGIR/lowering | Concrete literal bits, row/format/width, operation summary, direct completion successors | immutable Sema operation facts |
| codegen | Exact LLVM type mapping, `sadd.with.overflow`, constrained `fadd`, capability gate | verified SGIR only |
| diagnostics/IDE/cache | Stable classification, hover/type facts, canonical cache keys | immutable semantic facts only |

Dependencies point downward. Parser, IDE, optimizer, and backend do not host alternate relation or conversion logic.

## Exact literal domain

```cpp
struct ExactInteger {
  llvm::APSInt value; // signed mathematical value, canonical zero
};

struct ExactDecimal {
  bool negative;
  llvm::APInt coefficient; // non-negative magnitude
  int64_t exponent10;      // checked base-ten exponent
  bool explicit_negative_zero;
};

using ExactLiteral = std::variant<ExactInteger, ExactDecimal>;
```

Decimal meaning is `(-1 if negative else 1) * coefficient * 10^exponent10`. The parser derives exponent from grammar-admitted fractional digits; this plan does not add exponent source syntax. Canonicalization strips redundant leading coefficient zeros and trailing zeros while adjusting `exponent10`, uses one canonical zero coefficient/exponent, and retains `explicit_negative_zero` only for decimal zero. Canonical serialization is sign/kind/coefficient-limb/exponent data, not locale-sensitive text.

`CompilationSession` owns an expression-keyed immutable `ExactLiteralFact` side table. AST nodes keep source spelling/location for diagnostics and formatting but carry no selected width/format. Facts are arena/session-owned, contain no raw AST pointer in persistent identity, and can be shared read-only after publication.

## Deterministic limits and work accounting

`NumericLiteralLimitConfig` contains five finite positive checked-in values:

- `max_numeric_token_bytes`
- `max_significant_digits`
- `max_exact_integer_bits`
- `max_abs_decimal_exponent`
- `max_literal_work_units`

The defaults are versioned as part of the semantic-cache configuration. Execution freezes concrete values only after the checked-in benchmark corpus records throughput and peak memory; enablement is impossible while any value is absent. Tests use the compiled configuration rather than duplicating constants.

The lexer charges bytes/digits while scanning and rejects before APInt allocation. Parsing uses chunked coefficient accumulation with a checked conservative bit estimate. `LiteralWorkBudget::charge(kind, units)` uses saturating `uint64_t` arithmetic; exact add/alignment, power-of-ten expansion, APFloat conversion, and folding precharge from digit/limb counts and exponent distance before allocating expanded storage. Charge formulas are deterministic upper bounds documented beside code. A cache hit revalidates the configuration fingerprint. Wall time, allocator exceptions, RSS sampling, and host word size never select acceptance.

Complexity goals are linear token scan/canonicalization, storage proportional to accepted coefficient/integer limbs, bounded constant-fold work, and O(1) finite catalog lookup.

## Materialization and defaults

```cpp
MaterializationResult materialize(
    const ExactLiteral&, ConcreteScalarType, LiteralWorkBudget&);

DefaultResult defaultAtConcreteBoundary(
    const ExactExpressionFact&, ConcreteBoundaryKind, LiteralWorkBudget&);
```

`MaterializationResult` contains either canonical concrete bits/format or one classified failure; it never returns a suggested wider type. Integer range checks use signed APInt/APSInt bounds. Integer-to-float conversion uses the target APFloat semantics and succeeds only when conversion status/value proves exactness. Decimal-to-float uses explicit nearest-ties-to-even and accepts inexact finite/subnormal results, preserves explicit negative zero, and rejects finite-source infinity. Decimal-to-integer is an immediate classified error.

Expected types and closed constraints invoke `materialize` first. `defaultAtConcreteBoundary` is callable only for an enumerated ordinary storage/non-generic-return boundary and selects `i64`/`f64`; a generalization context is not a boundary and has no default API. Out-of-default-range is a diagnostic, not automatic `i128` selection.

## Closed `Add` catalog and Q02 seam

`BuiltinAddCatalog` is an immutable dense table indexed by the seven concrete scalar ordinals and two exact literal kinds. Each `BuiltinAddRow` has a stable versioned row ID, operand shapes, success type, materialization predicate, operation kind, and finite completion set. Concrete/concrete lookup is an exact ordinal equality check. Literal/concrete and concrete/literal use the same materializer and symmetric table entries. There is no catch-all row.

Public queries are deliberately narrow:

```cpp
AddQueryResult queryAdd(OperandDomain left, OperandDomain right,
                        LiteralWorkBudget&);
CompletionSet conservativeCompletionBound(AddConstraintShape);
CatalogFingerprint builtinAddCatalogFingerprint();
```

Q02 adapts its own type terms to `OperandDomain`, stores its own constraint, and calls these immutable queries. Q05 never constructs a Q02 `TypeVar`, scheme, instance, or specialization key. The union of legal signed rows `{overflow}` and floating rows `{}` is `{overflow}`; the catalog exposes this stable upper bound without instance-dependent narrowing. Catalog serialization is sorted by stable row ID and fully compared after hash matches.

Excluded families return classified `NoRow`; string, matrix, and container code is not called as a scalar fallback. If a separately owned operation already exists, it must have a distinct operation identity and dispatch before/after this catalog under that owner's contract; Q05 does not invent its row.

## Operation descriptor, constant evaluation, and SGIR

One selected row produces immutable `NumericOperationDescriptor { row_id, concrete_type, operation_kind, completion_set }`. The constant evaluator and typed lowering consume the same descriptor.

- Integer constants use APInt at the selected width and a signed overflow check. Success returns exact width bits; overflow returns the resolved payload-free `overflow` completion outcome.
- Floating constants use APFloat with the row's `fltSemantics`, `rmNearestTiesToEven`, and operation status. Infinity/NaN remain successful floating values. No host `long`, `double`, current rounding environment, string round-trip, or C++ signed overflow participates.
- Exact literal-literal simplification remains an exact provisional fact. Once a row/default is selected, operands materialize first and the evaluator executes that row, preventing fold-time bypass.

SGIR constants carry concrete integer width or floating format and canonical bits. Numeric binary IR carries the row ID and `OperationSummary`. Verification checks operand/result equality, known row ID, materialization completion, `{overflow}` plus two successors for signed rows, empty completion set for floating rows, and absence of unresolved/`Undefined` state. Generic settlement constructs are consumed from their existing owner; Q05 adds no second CFG framework.

## Backend contract

`GetTypeG` maps `i8/i16/i32/i64/i128` to exact LLVM integer types and `f32/f64` to LLVM float/double. Missing or unresolved mappings are verifier/compiler errors, never `i64`/double defaults.

Signed `Add` emits `llvm.sadd.with.overflow.iN`, extracts `{sum, overflow_bit}`, and branches directly to SGIR's success or `overflow` successor. It applies neither `nsw` nor `nuw` and never emits a naked language integer `CreateAdd`.

Floating `Add` emits `llvm.experimental.constrained.fadd` with explicit nearest-ties-to-even and the chosen exception behavior, under `strictfp` and IEEE denormal attributes. The emitted operation has no fast-math flags. The target capability gate records triple, CPU/features, LLVM version, constrained-FP support, and denormal contract. Unsupported targets select only a separately verified repository-owned deterministic helper or reject compilation with the strict-FP-target diagnostic; ordinary `fadd`, FTZ/DAZ, or host rounding is never fallback behavior.

An IR audit at `-O0`, `-O2`, `-O3`, and LTO verifies intrinsic/attributes, no forbidden flags, and bit-pattern/runtime equivalence.

## Diagnostics, IDE, and cache identity

`NumericDiagnosticKind` has stable categories for resource limits, spelling/canonicalization, materialization, default range, concrete mismatch, excluded family/no row, unhandled overflow, SGIR invariant failure, and strict-FP target rejection. Each includes source range, operand/literal facts safe for display, selected target/row where available, and bounded notes. The `overflow` edge itself is not misreported as a type error; existing settlement analysis reports an unhandled completion.

CLI and `CompilerBridge` consume the same diagnostic/fact record. Hover shows exact/unmaterialized state only where useful during analysis and the selected concrete type at a boundary; it never presents that compiler fact as an author-visible exact runtime type.

Cache keys include compiler/schema version, canonical exact encoding, expected/concrete boundary, row/catalog fingerprint, completion-family identity, limit-config fingerprint, and strict-FP target capability digest. Full canonical values are compared on hash collision. Session-local IDs/raw pointers/unordered iteration are excluded. Facts are immutable after publication, so independent files/call uses may read concurrently; session tables serialize publication through existing infrastructure.

## One-shot migration and deletion

The convergence checkpoint inventories every current route, then changes authority once:

1. parser/AST source spelling feeds exact semantic facts;
2. `TypeInfer` scalar `Add` calls only materialization/catalog queries;
3. lowering and optimizer consume selected descriptors and concrete bits;
4. SGIR verification rejects repairable ambiguity;
5. backend consumes only verified rows;
6. IDE/cache consume the same semantic facts;
7. old promotion, string/numeric, host conversion, unchecked add, mixed-float, and default repair code plus dependent tests are removed.

Searches are scoped to scalar language `Add` and numeric literals. Independent matrix/container operations are retained only if they have a separate owner/operation identity; otherwise current unauthorized `+` acceptance rejects pending its future owner. No compatibility wrapper, dormant flag, copied helper, or obsolete fixture remains.

## Concurrency and failure isolation

Limit configuration, catalog, and target contract are immutable per session. Exact values and selected operation facts are value objects. Parser/Sema work for independent expressions can run concurrently with separate `LiteralWorkBudget` counters. APInt/APFloat temporaries remain request-local. Resource failure, materialization failure, no-row failure, arithmetic completion, and unsupported target are disjoint outcomes and cannot be converted into one another.
