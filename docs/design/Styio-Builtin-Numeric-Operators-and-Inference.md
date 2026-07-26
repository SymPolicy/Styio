# Styio Built-in Numeric Operators and Inference

**Purpose:** Define the accepted `Q05-NUMERIC-OPS` built-in scalar domain,
lossless implicit widening, mixed arithmetic result inference, exact
heterogeneous comparison, operator completion sets, and compound-assignment
commit boundary.

**Last updated:** 2026-07-26

**Status:** Accepted owner decision `Q05-NUMERIC-OPS` on 2026-07-26.

## 1. Scope and authority

This document is the sole detailed semantic owner for built-in numeric
operator admission, operand/result inference, implicit widening, and numeric
compound assignment.

It replaces the former same-concrete-type restrictions in `Q05-LIT-ADD`,
`Q05-SCALAR-CONV`, and `Q05-INT-DIVREM`, and absorbs the proposed
`Q05-FLOAT-DIV` package. The following documents retain narrower ownership:

- [Styio Exact Numeric Literals](./Styio-Exact-Literals-and-Builtin-Add.md)
  owns exact integer/decimal terms, contextual materialization, late defaults,
  and compiler resource limits; its historical Add rows are replaced here.
- [Styio Checked Scalar Conversion](./Styio-Checked-Scalar-Conversion.md)
  owns the author-visible `expr :> T` spelling and checked non-widening
  conversion failures.
- [Styio Euclidean Signed-Integer Division and
  Remainder](./Styio-Euclidean-Signed-Integer-Division-and-Remainder.md)
  owns the Euclidean quotient/remainder invariant and its exceptional values;
  operand promotion and result typing are owned here.
- Q01 owns operation summaries and settlement, Q02 owns principal inference,
  Q03-F owns evaluation/order, and Q04 owns mutation-place lifetime and
  ownership.

No new numeric operator token is introduced. The accepted unsigned canonical
type identities use ordinary type-namespace names and do not become lexer
keywords or callable expression heads. This decision removes `**` from target
syntax.

## 2. Closed scalar domain and source surface

The built-in runtime numeric domain is:

```text
SignedInteger = {i8, i16, i32, i64, i128}
UnsignedInteger = {u8, u16, u32, u64, u128}
FixedInteger  = SignedInteger union UnsignedInteger
Float         = {f32, f64}
NumericScalar = FixedInteger union Float
```

There is no canonical platform-width `int`, `uint`, `isize`, or `usize`.
`u8` is both the eight-bit unsigned numeric type and the octet element used by
`bytes`; there is no separate scalar `byte`.

The admitted numeric operator surface is:

```text
unary:        -x
arithmetic:   x + y, x - y, x * y, x / y, x % y
comparison:   x == y, x != y, x < y, x <= y, x > y, x >= y
conversion:   x :> T
compound:     place += y, place -= y, place *= y, place /= y
```

`!`, `&&`, and `||` remain Boolean-only logical operators. There is no numeric
truthiness.

The following have no target-language numeric role:

- unary `+`;
- `++` or `--`;
- infix power `**` or compound `**=`;
- compound remainder `%=`;
- numeric bitwise NOT/AND/OR/XOR or shifts under `~`, `&`, `|`, `^`, `<<`,
  or `>>`; glyphs already owned by nonnumeric Styio forms keep those forms;
- logic XOR `⊕`; and
- author-defined operator rows or open overload search.

## 3. Four distinct numeric mechanisms

Styio does not model every numeric interaction as a hidden cast. Four
mechanisms remain distinct:

1. An exact literal receives contextual materialization from the literal
   owner.
2. A concrete value may use a total, lossless `Widen(Source, Target)` relation.
3. A numeric operator selects one closed heterogeneous operation row and its
   result type.
4. An author may request checked conversion with `expr :> T`.

This distinction is observable. For example, `i64_value :> f64` may complete
with `inexact`, while `i64_value + f64_value` is a mixed floating operation
whose correctly rounded `f64` result is ordinary floating arithmetic.

## 4. Total lossless implicit widening

Ordinary assignment, argument passing, return adaptation, and branch/result
joining may use only the following total value-preserving relation:

| Source | Additional implicit targets |
|---|---|
| `i8` | `i16`, `i32`, `i64`, `i128`, `f32`, `f64` |
| `u8` | `u16`, `u32`, `u64`, `u128`, `i16`, `i32`, `i64`, `i128`, `f32`, `f64` |
| `i16` | `i32`, `i64`, `i128`, `f32`, `f64` |
| `u16` | `u32`, `u64`, `u128`, `i32`, `i64`, `i128`, `f32`, `f64` |
| `i32` | `i64`, `i128`, `f64` |
| `u32` | `u64`, `u128`, `i64`, `i128`, `f64` |
| `i64` | `i128` |
| `u64` | `u128`, `i128` |
| `i128` | none |
| `u128` | none |
| `f32` | `f64` |
| `f64` | none |

Identity is always implicit. Transitive paths denote the same unique target,
not a sequence selected by source order.

Every integer value admitted by an integer-to-float row above is exactly
representable in the target format. `f32` to `f64` preserves every finite
value, infinity, signed zero, and NaN value class. NaN payload/sign and
signaling state are not part of this widening promise.

Implicit widening has an empty completion set. Signed-to-unsigned flow,
same-width unsigned-to-signed flow, narrowing, float-to-integer, 32-bit
integers to `f32`, 64/128-bit integers to a float, and `f64` to `f32` are not
ordinary value-flow widenings. They require an admitted operator row or
explicit `:>` conversion.

Expected types may select a unique widening target but may not cause
value-dependent narrowing, silently lossy conversion, or backend repair.

## 5. Binary arithmetic result inference

For two concrete integer operands, `CommonInt(I, J)` is selected as follows:

1. two signed operands use the wider signed width;
2. two unsigned operands use the wider unsigned width;
3. a signed `iN` and unsigned `uM` use the smallest available signed `iW`
   for which `W >= N` and `W > M`.

Both operands widen losslessly to that result type. If no fixed signed type
can represent both complete operand domains, the arithmetic row does not
exist. Thus `i64 + u32 -> i64`, `i64 + u64 -> i128`, and
`i128 + u64 -> i128`, while `i64 + u128` and `i128 + u128` are type errors.
Comparison remains available even when arithmetic has no common fixed type.

For an operation with at least one floating operand, `FloatResult(I, F)` is
selected symmetrically by this table:

| Integer side | `f32` side | `f64` side |
|---|---|---|
| `i8`, `u8`, `i16`, or `u16` | `f32` | `f64` |
| `i32`, `u32`, `i64`, `u64`, `i128`, or `u128` | `f64` | `f64` |

Two floating operands use their wider format. Consequently:

```text
i32 + i64   -> i64
i32 + u32   -> i64
u32 + u64   -> u64
i64 + u64   -> i128
f32 / f64   -> f64
i16 * f32   -> f32
i32 - f32   -> f64
u128 + f64  -> f64
```

The result type depends only on the operator and solved operand types. A
downstream expected type cannot select a narrower row or change rounding.

Exact literals continue to use their accepted contextual materialization and
late-default rules. Once materialized, they enter the same concrete relation;
an optimizer cannot use the literal's compile-time representation to select a
different runtime row.

## 6. Correctly rounded mixed integer/floating arithmetic

A mixed integer/floating `+`, `-`, `*`, or `/` row is a heterogeneous
operation, not an implicit insertion of `:>`.

For finite operands:

1. the integer contributes its exact mathematical integer;
2. the floating operand contributes the exact dyadic value encoded by its
   source format;
3. the mathematical operation is performed exactly; and
4. the final result is rounded once to the inferred `f32` or `f64` format,
   using round-to-nearest, ties-to-even.

This rule forbids operand-rounding followed by a second arithmetic rounding.
For example:

```styio
a: i64 = 9007199254740993
b: f64 = -9007199254740992.0
a + b  // 1.0
```

NaN, infinity, and signed-zero cases follow the corresponding strict IEEE
operation after treating every integer as a finite exact value and integer
zero as unsigned mathematical zero. A NaN input produces a quiet NaN value
class. Finite overflow produces signed infinity; gradual underflow produces a
subnormal or signed zero.

Floating rows have an empty completion set. Rounding, overflow to infinity,
underflow, NaN, and division by floating zero are floating values, not
`inexact`, `overflow`, `non_finite`, or `divide_by_zero` completions.

`f32` computation stays `f32` when the table selects it. `f32` to `f64`
widening is exact at the value-class boundary. Implementations may not read
the host rounding mode, expose host exception flags, enable FTZ/DAZ, attach
result-changing fast-math flags, reassociate operations, or contract separate
multiply/add operators into FMA unless bit-for-bit equivalence is proven for
that concrete expression.

## 7. Complete admitted numeric operator table

| Operator | Operand row | Result | Static completion set |
|---|---|---|---|
| unary `-` | signed integer `I` | `I` | `{overflow}` |
| unary `-` | unsigned integer | no row; explicit checked conversion to a signed type is required | n/a |
| unary `-` | float `F` | `F` | `{}` |
| `+` | integers `I`, `J` with `CommonInt` | `CommonInt(I,J)` | `{overflow}` |
| `-` | integers `I`, `J` with `CommonInt` | `CommonInt(I,J)` | `{overflow}` |
| `*` | integers `I`, `J` with `CommonInt` | `CommonInt(I,J)` | `{overflow}` |
| `/` | integers `I`, `J` with `CommonInt` | `CommonInt(I,J)` | unsigned result: `{divide_by_zero}`; signed result: `{divide_by_zero, overflow}` |
| `%` | integers `I`, `J` with `CommonInt` | `CommonInt(I,J)` | `{divide_by_zero}` |
| `+`, `-`, `*`, `/` | row containing a float | inferred float | `{}` |
| `%` | row containing a float | no row; type error | n/a |
| `==`, `!=`, `<`, `<=`, `>`, `>=` | any two numeric scalars | `bool` | `{}` |

Integer unary negation completes with `overflow` only for `MIN_I`.
Integer `+`, `-`, and `*` use checked arithmetic in their inferred result
type; an unrepresentable mathematical result completes with payload-free
`overflow`.

For integer `/` and `%`, operands first widen losslessly to
`CommonInt(I,J)`. A signed result then uses the Euclidean invariant:

```text
a = q*b + r
0 <= r < abs(b)
```

`/` returns `q`; `%` returns `r`. Division or remainder by zero completes with
payload-free `divide_by_zero`. `MIN_W / -1` completes with `overflow`, while
`MIN_W % -1` succeeds with zero. Complete quotient/remainder details remain in
the Euclidean owner.

An unsigned result uses the ordinary nonnegative quotient and remainder with
`a = q*b+r` and `0 <= r < b`; only a zero divisor completes. A mixed
signed/unsigned row, when it exists, has a signed common result and therefore
uses the Euclidean rule.

The `-` in a negative exact literal belongs to the literal term before
materialization. Runtime unary negation is the checked/IEEE row above.

## 8. Exact heterogeneous numeric comparison

Comparison accepts every pair in `NumericScalar × NumericScalar`; it does not
require a common storage type and never rounds an integer into a float.

- Integer/integer comparison is exact and sign-aware. Two same-signedness
  operands may widen normally; a negative signed value is below every unsigned
  value, and nonnegative mixed values compare by magnitude without lossy
  conversion.
- `f32`/`f64` comparison widens `f32` exactly to the `f64` value domain.
- Integer/float comparison compares the mathematical integer directly with
  the exact dyadic value encoded by the float. An implementation may compare
  signs, exponent, significand, integer bit length, and discarded fractional
  bits, but may not first cast the integer to a float.
- Integer zero, `+0.0`, and `-0.0` compare equal.
- If either operand is NaN, `==` is false, `!=` is true, and `<`, `<=`, `>`,
  and `>=` are false.
- Negative infinity is below every finite numeric value and positive infinity
  is above every finite numeric value.

Therefore:

```styio
30.0 > 29  // true

a: i64 = 9007199254740993
b: f64 = 9007199254740992.0
a == b  // false
a > b   // true
```

These are quiet value comparisons with empty completion sets. This decision
does not add a total-order operator, bit-pattern equality, or numeric hashing.
A later hash decision must preserve the equality of all cross-type values that
compare equal here.

## 9. Checked conversion remains distinct

`expr :> T` remains the only author-visible checked scalar-conversion syntax.
It is required when ordinary value flow lacks a lossless widening row and the
author wants a concrete target.

Because the numeric value model now admits total `f32` to `f64` widening,
cross-format NaN conversion preserves the NaN value class and succeeds; its
payload/sign/signaling details are unspecified. The checked-conversion owner
defines the resulting revised completion matrix. `non_finite` remains
applicable where a non-finite float cannot enter an integer target.

No `T(expr)`, `cast[T]`, user conversion trait, hidden backend cast, or
conversion keyword is introduced.

## 10. Numeric compound assignment

`+=`, `-=`, `*=`, and `/=` are true numeric read-modify-write operations:

1. the left side resolves to one mutable numeric place;
2. its old value and the right operand are each evaluated/read exactly once
   under Q03-F and Q04;
3. the corresponding binary row is selected;
4. the result must flow losslessly back to the left type; and
5. only complete success commits one write and yields `unit`.

If operand evaluation or the operation completes, the write does not occur
and the old value remains installed. There is no automatic narrowing or
wraparound on write-back.

Examples:

```text
i64_place += i32_value  accepted
u64_place += u32_value  accepted
i128_place += u64_value accepted
f64_place += i64_value  accepted; mixed result is f64
i32_place += i64_value  rejected; result is i64
u64_place += i64_value  rejected; mixed result is i128
f32_place += i32_value  rejected; result is f64
```

Integer compound rows expose the completion set of the underlying operation.
Floating `/=` stores infinity, signed zero, or NaN normally when the IEEE row
produces it. `+=` has no contextual string, container, matrix, stream, or
reduction role. `%=` and `**=` are not admitted.

## 11. Inference and completion bounds

The compiler may display closed metalanguage relations such as:

```text
Widen(Source, Target)
NumericBinary(Op, Left, Right, Result, CompletionSet)
NumericCompare(Op, Left, Right, bool, {})
```

They are not source syntax, traits, or runtime dictionaries.

Q02 may retain these finite constraints in an eligible principal scheme. Each
use is freshly instantiated and must select one row before SGIR. A scheme
spanning rows with different completion sets uses their finite conservative
union. Thus a generalized `x + 5` constraint that includes integer and float
rows retains `{overflow}` even when one concrete instance is floating.

Implicit widening never contributes a completion family. A selected explicit
conversion or numeric operation contributes exactly its accepted finite set.

## 12. Constants, evaluation, and optimization

Operands are strict values and are evaluated exactly once under Q03-F.
Independent operands do not gain a source-left-to-right time edge; unordered
order-sensitive siblings fail closed.

Constant evaluation and runtime execution select the same row and must produce
the same non-NaN value bits or completion. For NaN, they agree on value class
and any separately frozen canonicalization fact, not an unspecified payload.

The finite type domain permits O(1) row selection from immutable tables.
Exact integer/float comparison can use fixed-width sign, exponent,
significand, and bit-length operations without heap-allocating an arbitrary
runtime number. Correctly rounded mixed arithmetic must use a bounded,
independently tested algorithm appropriate to the fixed `i128`/binary64
maximum; a host cast followed by a host operation is not a semantic oracle.

Optimizers may fold or replace an operation only after proving identical row,
result type, completion set, rounding point, special-value behavior, and
evaluation facts.

## 13. One-way implementation migration

One implementation lifecycle must converge the entire accepted numeric
surface. It must:

- replace generic `getMaxType`/backend guessing with the closed widening,
  binary, comparison, and conversion facts;
- migrate exact literals, Add, explicit conversion, Euclidean division and
  remainder, float division, comparison, and compound assignment together;
- replace integer-to-`double` comparison lowering with exact heterogeneous
  comparison;
- add correctly rounded mixed arithmetic rather than cast-then-operate repair;
- remove `**` from lexer, parser, AST, Sema, IR, constant folding, codegen,
  tests, editor grammar, and documentation;
- remove inactive numeric unary-plus, bitwise, shift, XOR, `%=` and power
  routes instead of retaining internal compatibility entries;
- delete `Bool_To_Int`, fixed `Int_To_Float`, numeric-string coercion, unchecked
  SGIR casts, host-number folding, and backend-created operator semantics where
  they conflict with this owner; and
- classify ABI, IO, container, matrix, allocator, scheduler, and runtime
  arithmetic by their independent owners before changing them.

There is no feature flag, legacy operator mode, alternate promotion table,
debug/release semantic fork, or long-term removed-syntax implementation gate.

## 14. Deferred and excluded

- platform-width aliases and additional floating formats;
- decimal/rational/big-number runtime types;
- named round, truncate, floor, ceil, saturate, wrap, and bit reinterpretation;
- total float order, NaN payload/canonicalization, numeric hash, and bitwise
  equality beyond the value-class facts required here;
- string concatenation and numeric parsing/formatting;
- product, container, matrix, series, SIMD, and broadcast operator rows;
- user-defined conversions, operators, traits, priorities, and open instances;
  and
- alternate fast floating profiles, host rounding modes, status flags, or
  traps.
