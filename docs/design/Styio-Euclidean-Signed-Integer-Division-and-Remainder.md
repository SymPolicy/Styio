# Styio Euclidean Signed-Integer Division and Remainder

**Purpose:** Define the accepted `Q05-INT-DIVREM` closed relations,
Euclidean quotient/remainder invariant, exact completion sets, and
constant/runtime equivalence for built-in signed integers.

**Last updated:** 2026-07-26

**Status:** Accepted invariant owner decision `Q05-INT-DIVREM` (scheme A) on
2026-07-25; operand and result inference revised by accepted
`Q05-NUMERIC-OPS` on 2026-07-26.

## 1. Scope and authority

This document is the sole detailed owner for the Euclidean signed-integer
quotient/remainder invariant and its operation-specific completions.
[Styio Built-in Numeric Operators and
Inference](Styio-Builtin-Numeric-Operators-and-Inference.md) owns the operand
and result rows, lossless widening, heterogeneous operations, compound
assignment, and the unified implementation lifecycle.

The decision closes exactly:

- the Euclidean meaning of admitted built-in signed-integer `/` and `%` rows;
- exact-integer-literal interaction;
- the quotient and remainder selected for every nonzero divisor;
- division-by-zero and unrepresentable-quotient completion behavior;
- the distinct finite completion set of each operation;
- definition-site inference, exact-once evaluation, constant evaluation, and
  optimization boundaries; and
- one-way removal of conflicting heuristic promotion, host-arithmetic, truncating,
  fallback-value, and backend-selected behavior for these two operations.

It does not independently admit floating-point division or remainder,
unsigned or platform-width integers, vectors, containers, matrices,
user-defined rows, power, wrapping, saturating, truncating, or floor-named
variants. The unified numeric owner admits strict-IEEE floating and mixed `/`
and the `/=` read-modify-write form; floating and mixed `%` remain absent.

## 2. Existing source form and parse

The decision adds no token or grammar production. It gives the existing infix
forms their signed-integer meaning:

```styio
quotient = dividend / divisor
remainder = dividend % divisor
```

`/` and `%` remain left-associative multiplicative operators with the same
precedence as `*`, above additive forms and below conversion `:>`. There is no
power operator. Parsing establishes only the expression tree; operand
evaluation remains governed by Q03-F.

`/=` is the separate read-modify-write operation accepted by the unified
numeric owner. `%=` is not admitted. Infix `/` and `%` do not themselves imply
mutation, atomicity, commit, or rollback.

## 3. Closed operand and result relation

The concrete domain is:

```text
SignedInteger = {i8, i16, i32, i64, i128}
```

For every ordered pair `I, J` in `SignedInteger`, let `WiderInt(I, J)` be the
narrowest member of `SignedInteger` that losslessly contains both operand
types. The catalog contains:

```text
I / J -> WiderInt(I, J)
I % J -> WiderInt(I, J)
```

Both operands are losslessly widened to `W = WiderInt(I, J)` before the
Euclidean operation, so `i32 / i64 -> i64` and `i64 % i128 -> i128`. This is a
finite built-in catalog relation, not a generic `getMaxType` heuristic. There
is no string coercion, `bool`, `char`, `byte`, container, matrix, user
instance, or backend-created fallback row.

Exact literals materialize under the separately owned contextual selection and
late-default rules before this concrete relation is selected. An out-of-range
materialization is a static literal diagnostic. Floating and mixed `/` are
owned by the unified numeric decision and use strict IEEE semantics; floating
and mixed `%` have no row.

## 4. Euclidean quotient and remainder

For mathematical integers `a` and nonzero `b`, the operations select the
unique mathematical integers `q` and `r` satisfying:

```text
a = q*b + r
0 <= r < abs(b)
```

Then:

```text
a / b = q
a % b = r
```

The equation and `abs(b)` are unbounded specification metalanguage. They do
not require source evaluation of `abs(MIN_T)` in `T`, expose a storage
representation, or let source multiplication/addition bypass their own
operation contracts.

The four sign combinations are:

| `a` | `b` | `a / b` | `a % b` |
|---:|---:|---:|---:|
| `7` | `3` | `2` | `1` |
| `-7` | `3` | `-3` | `2` |
| `7` | `-3` | `-2` | `1` |
| `-7` | `-3` | `3` | `2` |

The remainder is therefore always nonnegative and is independent of the sign
of the divisor.

## 5. Completion families and exceptional mathematical inputs

The prelude supplies the ordinary payload-free nominal completion family
`divide_by_zero`. It is a resolved identifier, not a keyword, exception
object, trap, or value wrapper. A settlement arm cannot bind a payload to it.

For either operation:

```text
a / 0  -> divide_by_zero
a % 0  -> divide_by_zero
```

For the inferred fixed-width signed result type `W`, the mathematical quotient
of `MIN_W / -1` is not representable in `W`. Division therefore produces the
already accepted payload-free `overflow` completion. Every other nonzero
divisor has a representable Euclidean quotient.

Remainder is independent:

```text
MIN_W % -1 = 0
```

That result succeeds. `%` does not inherit an `overflow` completion merely
because a hardware instruction, backend IR, or shared quotient/remainder
implementation couples it to the unrepresentable quotient.

## 6. Exact finite relations and inference boundary

The canonical internal relations may be displayed as:

```text
IntDiv(I, J, WiderInt(I, J), {divide_by_zero, overflow})
IntRem(I, J, WiderInt(I, J), {divide_by_zero})
```

This is compiler/specification metalanguage, not author-written generic,
overload, constraint, or completion-row syntax. Each relation has 25 ordered
rows, one for every admitted pair `(I, J)`.

Every selected concrete row contributes its result and exact completion set to
the existing Q01-A algebra:

```text
OperationSummary(T, CompletionSet)
```

Q02-INF may retain only these closed relation constraints in an eligible
definition-site principal scheme. Its candidates come solely from the finite
catalog, and any unresolved legal rows use the finite conservative union of
their completion sets. A later concrete call cannot add a row, narrow the
stable visible completion bound, or infer from future calls.

## 7. Evaluation and completion edges

The left and right operands are strict prerequisites and each denotes one
logical evaluation under Q03-F. The division or remainder operation begins
only after both operands have produced normal values. Their source positions
do not create an additional left-to-right time edge.

The operation then has exactly these outcomes:

| Operation | Success | Completion |
|---|---|---|
| `/` | Euclidean quotient `W` | `divide_by_zero` when `b = 0`; `overflow` when widened `a = MIN_W` and widened `b = -1` |
| `%` | Euclidean remainder `W`, including `MIN_W % -1 = 0` | `divide_by_zero` when `b = 0` |

A non-empty completion set makes the node order-sensitive under Q03-F.
Settlement and propagation remain owned by Q01-A. No path returns a sentinel,
zero fallback, Optional, `Result`, panic, environment exception, or
debug/release-specific substitute.

## 8. Constant/runtime identity

Constant evaluation and runtime execution select the same concrete row and
produce the same success value or nominal completion family. A statically
known zero divisor or `MIN_T / -1` follows the corresponding completion edge;
it is not an optimization-only type error and cannot be evaluated with host
`long`, `long long`, `/`, or `%` semantics.

An implementation may derive a Euclidean result from a guarded truncating
primitive, but the observable relation remains this document's relation. In
particular, after excluding invalid primitive inputs, a truncating quotient
`q0` and remainder `r0` require correction whenever `r0 < 0`:

```text
if b > 0: q = q0 - 1; r = r0 + b
if b < 0: q = q0 + 1; r = r0 - b
```

This is a permitted implementation identity, not source syntax. It must never
compute `abs(MIN_T)` in `T`. For `%`, `MIN_T/-1` must bypass any backend
remainder primitive whose contract also rejects the unrepresentable quotient.

## 9. Optimization and lowering boundary

Constant folding, strength reduction, common-subexpression elimination,
vectorization, and target-specific instruction selection must preserve:

- the Euclidean rather than truncating quotient/remainder;
- the exact `divide_by_zero` and `overflow` successors;
- exact-once operand evaluation and completion stop;
- the independent successful result `MIN_T % -1 = 0`; and
- every fixed-width result bit pattern implied by the mathematical invariant.

An unchecked LLVM `sdiv` or `srem` cannot define the language operation.
Backend instructions may execute only on a control-flow path that has already
excluded every input their backend contract rejects. `%` cannot be replaced
unconditionally by fixed-width `a - (a / b) * b`; that expression can overflow
or take a completion edge even when the mathematical remainder is valid.

There is no fast, unchecked, debug, release, target, or compatibility mode
that changes these rules.

## 10. Examples

```styio
# quotient : i64 ?| {divide_by_zero, overflow} :=
    (a: i32, b: i64) => a / b

# residue : i128 ?| {divide_by_zero} :=
    (a: i64, b: i128) => a % b

normalized: i64 = ?| offset % period
                  | divide_by_zero => 0
```

Mixed signed integers select the wider signed result without author-written
conversion:

```styio
left_i32 / right_i64          // i64
left_i64 % right_i128         // i128
```

## 11. One-way implementation migration

The accepted implementation must replace or delete, in one converged
lifecycle, every infix signed-integer `/` or `%` authority that performs:

- generic `getMaxType`, backend ranking, or other non-catalog promotion;
- string, pointer, float, `char`, `bool`, matrix, or backend-only coercion;
- host-width literal parsing or host `/`/`%` constant evaluation;
- truncating negative quotient/remainder as the language result;
- sentinel, zero, `undef`, select, panic, trap, or environment-error fallback;
- one shared bad-input mask that gives `%` the quotient's overflow behavior;
- unchecked `sdiv`/`srem` before the language completion branch; or
- positive fixtures that preserve any of those obsolete semantics.

The migration must integrate this invariant with the unified numeric catalog
while separating author operators from matrix, internal-runtime, and other
independently owned uses. Classified external-owner paths are not silently
converted into this relation, and no compatibility flag or second semantic
route remains.

## 12. Deferred and excluded owners

- The unified numeric owner defines `-`, `*`, strict-IEEE floating and mixed
  `/`, exact mixed comparison, NaN comparison behavior, and `/=` commit
  semantics. It deliberately removes `**`.
- Floating and mixed `%`, unsigned/platform types, NaN payload preservation,
  and named wrapping/saturating/truncating/floor operations remain separate.
- `%=` is not admitted.
- Q08 owns container, matrix, series, broadcast, and shape-aware division or
  remainder.
- Runtime/library-internal division for allocation, indexing, scheduling, or
  statistics remains with its named subsystem owner and cannot masquerade as
  author infix semantics.
- UB, panic, environment exceptions, implicit floating true division,
  negative default remainders, and mode-dependent behavior are not admitted.
