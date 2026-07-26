# Styio Checked Scalar Conversion

**Purpose:** Define the accepted `Q05-SCALAR-CONV` source form, closed
conversion relation, exact-success policy, finite completion families, and
constant/runtime equivalence for built-in numeric scalars.

**Last updated:** 2026-07-26

**Status:** Accepted owner decision `Q05-SCALAR-CONV` (scheme B) on
2026-07-25, revised by accepted `Q05-NUMERIC-OPS` on 2026-07-26.

## 1. Scope and authority

This document is the sole detailed semantic owner for
`Q05-SCALAR-CONV`. Other design documents contain only grammar, token, or
summary mirrors and links to this owner.

The decision closes exactly:

- the one author-visible source form for converting a numeric scalar;
- the twelve admitted source and target scalar types;
- exact-literal interaction;
- the exact condition for runtime success;
- the finite completion set for every concrete source/target pair;
- definition-site inference, evaluation, constant-folding, and lowering
  boundaries; and
- separation from the accepted total implicit-widening relation and deletion
  of every conflicting backend-only or silently lossy conversion route.

It does not admit platform-width integers, text/code-point conversion,
product/container/matrix conversion, user-defined conversion, bit
reinterpretation, or an intentionally lossy round, truncate, saturate, or wrap
operation.

## 2. Source form and parse

The only checked scalar-conversion expression is:

```styio
value :> i64
```

The contiguous `:>` token separates one value expression on the left from one
scalar type on the right. Its grammar is equivalent to:

```ebnf
conversion_expr       = unary_expr [ ':>' scalar_conversion_type ] ;
scalar_conversion_type = identifier ;
```

Sema requires that identifier to resolve to the canonical identity of one of
`i8`…`i128`, `u8`…`u128`, `f32`, or `f64`. These names remain ordinary
type-namespace identifiers rather than lexer keywords.

`:>` has lower precedence than postfix calls/selectors/member access and unary
operators, and higher precedence than `**`, multiplicative, additive,
comparison, logical, guard, directional-transfer, and settlement forms. It is
non-associative:

```styio
left :> i64 + right       // (left :> i64) + right
(left + right) :> i64     // convert the complete sum
(value :> i64) :> i128    // an explicitly parenthesized second conversion
value :> i64 :> i128      // syntax error
```

After `:>`, the parser enters the closed scalar-type context above. The target
is not parsed as a value expression, callable, constructor, member, overload,
or generic application. In particular, this decision does not give `i64` or
another type name an expression-head role. `i64(value)`, `cast[i64](value)`,
and `convert[i64](value)` are not alternate spellings.

## 3. Closed source and target domain

The concrete runtime domain is:

```text
SignedInteger = {i8, i16, i32, i64, i128}
UnsignedInteger = {u8, u16, u32, u64, u128}
FixedInteger  = SignedInteger union UnsignedInteger
Float         = {f32, f64}
BuiltinScalar = FixedInteger union Float
```

The left side must be either an exact numeric literal term or a value already
typed as one member of `BuiltinScalar`. `bool`, `char`, `string`,
containers, matrices, handles, user types, and `Unknown` have no row and fail
statically before lowering.

The total value-preserving `Widen(Source, Target)` rows accepted by
[Styio Built-in Numeric Operators and
Inference](./Styio-Builtin-Numeric-Operators-and-Inference.md) may apply in
ordinary binding, argument, return, join, and operator contexts without
author-written syntax. They are not synthesized `:>` expressions, have empty
completion sets, and never perform narrowing or potential value loss.

An arithmetic neighbor may also select a heterogeneous numeric operation row;
that row is not an implicit conversion. Constant folding, SGIR repair, and
backend instruction selection cannot create any widening, operation, or
checked-conversion row.

## 4. Exact literals retain their accepted materialization rules

When the left side is still an exact numeric literal term, `:> T` supplies only
the target context. The materialization rules remain owned by
[Styio Exact Numeric Literals](./Styio-Exact-Literals-and-Builtin-Add.md):

- an integer literal entering a fixed integer must be in range;
- an integer literal entering `f32` or `f64` must be exactly representable;
- a decimal literal cannot materialize as a fixed integer;
- a decimal literal entering `f32` or `f64` uses round-to-nearest,
  ties-to-even and must not turn a finite source value into infinity; and
- signed-zero treatment remains the accepted exact-literal treatment.

A failed exact-literal materialization is a static materialization diagnostic,
not a runtime conversion completion:

```styio
1 :> i32          // exact-literal materialization to i32
1.0 :> i32        // static decimal-to-integer materialization error
```

## 5. Exact runtime success

For a concrete runtime scalar, conversion succeeds only when the target
preserves:

1. the mathematical numeric value;
2. finite versus positive-infinity versus negative-infinity classification;
3. applicable signed-zero identity; and
4. the accepted NaN boundary in section 6.

Consequently:

- same-type identity and every accepted integer widening always succeed;
- every other integer-to-integer row succeeds only in target range;
- fixed integer to float succeeds only when the target format represents the
  integer exactly;
- float to fixed integer succeeds only for a finite in-range integral value
  that is not negative zero;
- `f64` to `f32` succeeds only when the value is represented exactly;
- `f32` to `f64` preserves every non-NaN value exactly and every NaN value
  class under section 6; and
- positive and negative infinity are preserved across the two float formats.

An explicit conversion is not permission to truncate, round, clamp, saturate,
wrap, or reinterpret bits.

## 6. NaN boundary

Same-type identity leaves a NaN unchanged. Cross-format conversion preserves
the NaN value class and succeeds, while payload, sign, and signaling state are
unspecified. This is the same value boundary required by total `f32` to `f64`
widening and strict mixed floating operations.

This decision does not otherwise define NaN payload stability, bit equality,
hashing, or total ordering. Quiet numeric comparison is owned by
[Styio Built-in Numeric Operators and
Inference](./Styio-Builtin-Numeric-Operators-and-Inference.md).

Optimizers may not assume NaN or infinity is absent. They may not use a host or
backend cast whose NaN, infinity, signed-zero, or precision behavior differs
from this contract.

## 7. Completion families and classification

The prelude supplies three ordinary payload-free nominal completion-family
identities:

```text
out_of_range
inexact
non_finite
```

They are resolved identifiers, not keywords. Binding a payload in a settlement
arm is invalid.

Failure classification is deterministic and exclusive:

1. a NaN or infinity that the selected integer-target row cannot convert produces
   `non_finite`;
2. a finite value outside the target range produces `out_of_range`;
3. an in-range value that would lose a fractional part, precision, or
   negative-zero identity produces `inexact`.

One failed conversion produces exactly one family.

## 8. Closed completion matrix

The static relation carries only families that can occur for its concrete
source/target pair:

| Source to target | Static completion set |
|---|---|
| Same type | `{}` |
| Any accepted total integer `Widen(Source, Target)` row | `{}` |
| Any other fixed-integer to fixed-integer row | `{out_of_range}` |
| `i8`/`u8`/`i16`/`u16` to `f32` or `f64`; `i32`/`u32` to `f64` | `{}` |
| `i32`/`u32` to `f32`; `i64`/`u64`/`i128`/`u128` to either float | `{inexact}` |
| `f32` to `f64` | `{}` |
| `f64` to `f32` | `{out_of_range, inexact}` |
| `f32`/`f64` to any fixed integer | `{non_finite, out_of_range, inexact}` |

The canonical internal relation may be displayed as:

```text
Convert(Source, Target, Target, CompletionSet)
```

This is compiler/specification metalanguage, not author-written generic or
constraint syntax.

## 9. Operation and inference boundary

Every selected runtime row contributes:

```text
OperationSummary(Target, CompletionSet)
```

Q01-A continues to own that two-field algebra and `?|` settlement. A caller
must settle or propagate every family in the selected finite set.

Q02-INF may retain the closed `Convert` constraint in an eligible
definition-site principal scheme. Candidate rows come only from this document's
finite catalog. A scheme spanning multiple legal source rows uses the finite
conservative union of their completion sets; a later concrete instance does
not silently narrow the visible scheme. Every source and target is concrete
before SGIR.

No user-defined row, open trait, runtime dictionary, completion-row variable,
or future-call inference enters this relation.

## 10. Evaluation, constants, and optimization

The left value is evaluated exactly once under Q03-F. The conversion node runs
only after that value exists. A concrete row with an empty completion set may
be proven total; a non-empty row retains its completion edge and is
order-sensitive under the existing rules.

Constant evaluation and runtime execution use the same selected row and failure
classification. A known failure follows the same named completion edge as a
runtime failure. It does not become an optimization-only type error and cannot
produce an approximate value.

Constant folding, vectorization, LLVM casts, fast-math, or target-specific
instructions cannot change the success predicate, completion family,
signed-zero identity, or NaN boundary.

## 11. Examples

```styio
wide: i64 = small_i32 :> i64

# narrow : i32 ?| {out_of_range} := (x: i64) => x :> i32

whole: i32 = ?| measured_f64 :> i32
             | non_finite => 0
             | out_of_range => 0
             | inexact => 0
```

Concrete mixed-type arithmetic and comparison use the independently accepted
numeric operator rows:

```styio
left_i32 + right_i64          // i64 result through lossless integer widening
left_i64 < right_f64          // exact heterogeneous comparison
left_i64 :> f64               // explicit checked conversion, possibly inexact
```

## 12. One-way implementation migration

The unified numeric implementation must replace or delete, in one converged
lifecycle:

- `NumPromoTy::Bool_To_Int`;
- the fixed `NumPromoTy::Int_To_Float` route;
- internal-only `TypeConvertAST` policy that lacks a source target and
  completion contract;
- implicit concrete coercion outside the accepted total `Widen` catalog and
  heterogeneous operator rows;
- backend-only casts and result repair; and
- positive fixtures that preserve any of those obsolete semantics.

There is no compatibility flag, alternate AST-only conversion entry, or
backend fallback.

## 13. Deferred and excluded owners

- `Q05-NUMERIC-OPS` owns subtraction, multiplication, floating division,
  heterogeneous comparison, implicit widening, power removal, and compound
  assignment. Accepted signed-integer `/` and `%` retain their invariant in
  [Styio Euclidean Signed-Integer Division and
  Remainder](./Styio-Euclidean-Signed-Integer-Division-and-Remainder.md).
- Later Q05 decisions own platform types, NaN payload/total order/hash, and
  separately named lossy round/truncate/saturate/wrap operations.
- Q06 owns numeric parsing/formatting and `char`/`scalar`/string conversion.
- Q07/Q08 own product, container, and matrix element/broadcast conversion.
- F02/Q10 own any future user-defined conversion, open relation, generic type
  application, constructor, or namespace extension.
- Bit reinterpretation, implicit narrowing or silently lossy value-flow
  conversion, `T(expr)`, `cast[T]`, mode-parameterized universal casts,
  runtime panic, Optional, and `Result` conversion failures are not admitted.
