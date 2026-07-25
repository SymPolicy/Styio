# Styio Exact Literals and Built-in Add

**Purpose:** Define the accepted `Q05-LIT-ADD` semantics for exact numeric
literals, fail-closed materialization, the compiler-closed scalar `Add`
relation, late concrete defaults, and arithmetic completion facts.

**Last updated:** 2026-07-20

**Status:** Accepted owner decision `Q05-LIT-ADD` (scheme A, all ten rules) on
2026-07-20.

**See also:** [Styio Language Design](./Styio-Language-Design.md) section 3.2,
[Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md),
and [Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md).

## 1. Scope and authority

This document is the sole detailed semantic owner for `Q05-LIT-ADD`. Other
design documents contain only summary mirrors and links to this owner.

The decision closes exactly the relationship among:

- exact integer and decimal literal terms before storage materialization;
- the concrete signed-integer and floating-point scalar types;
- the compiler-owned finite `Add` relation;
- the payload-free nominal completion family `overflow`; and
- late defaulting at a boundary that requires one concrete value type.

It does not admit author-written constraints, an `Add` trait, open overload
search, a runtime numeric dictionary, new arithmetic types, or a conversion
syntax. The names `IntegerLiteral`, `DecimalLiteral`, `Add`, and `Completion`
used below are compiler/specification metalanguage, not Styio identifiers
written by authors.
`overflow` is different: it is an ordinary prelude completion-family
identifier, not a keyword.

The canonical internal relation display is `Add(L, R, S, C)`: left operand,
right operand, unique success type, and concrete finite completion set. `C` is a
projection of the closed row, not a quantified completion-row variable.

No keyword, token, AST source form, precedence rule, or EBNF production is
introduced by this decision. Existing literal and `+` spelling remains under
the formal grammar. Any radix, separator, or exponent spelling not already
admitted by that grammar remains unadmitted; this semantic decision does not
silently add it.

## 2. Rule 1: literals remain exact before materialization

Before a concrete storage type is selected, an integer literal denotes its
arbitrary-precision signed mathematical integer. Positive and negative source
forms enter the same exact term, so integer `-0` and `0` denote the same value.

A decimal literal retains an exact decimal coefficient and base-ten exponent.
It also retains whether an explicitly signed decimal zero was negative so that
source `-0.0` can materialize as IEEE negative zero. This representation is a
compiler fact, not a runtime arbitrary-precision numeric type and not an
author-visible hidden type.

Radix and separators, when independently admitted by the source grammar, affect
only source representation and not the mathematical value. Definition-site
generalization never chooses a storage width for an exact literal.

## 3. Rule 2: materialization is explicit and failure-closed

Materialization converts one exact literal term to a context-selected concrete
type. It follows this closed table:

| Exact source term | Target | Accepted only when | Result |
|---|---|---|---|
| `IntegerLiteral(v)` | `i8`, `i16`, `i32`, `i64`, or `i128` | `v` is within the target's signed range | the exact target value |
| `IntegerLiteral(v)` | `f32` or `f64` | the target IEEE format represents `v` exactly | that exact floating value |
| `DecimalLiteral(d)` | any signed integer | never implicitly | materialization error |
| `DecimalLiteral(d)` | `f32` or `f64` | IEEE rounding does not turn the finite source value into an infinity | round-to-nearest, ties-to-even |

Failure is a static materialization/type diagnostic. It never truncates,
wraps, saturates, silently widens, or changes the literal's kind. In particular,
`small: i8 = 128` is a materialization error, and an out-of-range value does not
cause the compiler to choose a wider type on its own.

Decimal-to-floating materialization preserves IEEE gradual underflow,
subnormal values, and signed zero. An explicitly written decimal `-0.0`
materializes as negative zero; exact integer zero has no negative-zero identity.
A finite decimal literal that would round to an infinity is rejected at the
materialization point.

## 4. Rule 3: `Add` is one finite compiler-owned scalar table

The concrete scalar domain is exactly:

```text
SignedInteger = {i8, i16, i32, i64, i128}
Float         = {f32, f64}
BuiltinScalar = SignedInteger union Float
```

The table's completion projection is also finite and closed:

```text
Completion(T) = {overflow}  when T is in SignedInteger
Completion(T) = {}          when T is in Float
```

For each `T` in `BuiltinScalar`, the closed relation contains these shapes:

| Left operand | Right operand | Admission condition | Success type | Completion set |
|---|---|---|---|---|
| `T` | `T` | both operands already have the identical concrete type | `T` | `Completion(T)` |
| `T` | `IntegerLiteral(v)` | the literal materializes to `T` under section 3 | `T` | `Completion(T)` |
| `IntegerLiteral(v)` | `T` | the literal materializes to `T` under section 3 | `T` | `Completion(T)` |
| `T` | `DecimalLiteral(d)` | `T` is `f32` or `f64` and the literal materializes to `T` | `T` | `{}` |
| `DecimalLiteral(d)` | `T` | `T` is `f32` or `f64` and the literal materializes to `T` | `T` | `{}` |

`bool`, `char`, `byte`, `string`, containers, and matrices are not members of
this table. An implementation path that currently accepts one of them with `+`
is not language authority. Adding a separately owned text, container, or matrix
operation later requires its own admitted closed relation.

Two still-exact literal terms may be simplified with exact arithmetic before a
concrete type is required. Such simplification remains provisional: after a
concrete row is selected, constant evaluation must be observationally identical
to materializing the operands and executing that row. Exact folding cannot
bypass a materialization failure, IEEE rounding, or integer `overflow`.

## 5. Rule 4: no C-style common-type promotion

Two operands that already have concrete numeric types are directly addable only
when their types are identical. `i32 + i64`, `i64 + f64`, and `f32 + f64` are
type errors. Styio performs no integer promotion, usual arithmetic conversion,
float/integer common-type selection, or backend-selected coercion for `Add`.

If exactly one operand is still an exact literal term, it may materialize to the
other operand's concrete type under section 3. This rule is symmetric in the
left and right positions. A future explicit conversion surface may make mixed
concrete expressions possible, but its spelling and conversion matrix are not
part of this decision.

## 6. Rule 5: the selected row has one result type

The selected scalar row uniquely determines the success type:

```text
T + T          -> T
T + Literal(v) -> T
Literal(v) + T -> T
```

There is no independent `Output` search and no result type inferred from a
downstream call. If no unique row exists, the expression is rejected. An
all-literal expression can remain exact while constraints are still being
solved; section 7 applies when a concrete value is required.

## 7. Rule 6: defaulting happens once, at a late concrete boundary

Context and expected types are applied before any default. If an exact literal
or all-literal expression is still unconstrained when an ordinary storage
binding, a non-generic return, or another boundary must produce one concrete
value type, Styio applies exactly one stable default:

| Exact term | Late default |
|---|---|
| integer | `i64` |
| decimal | `f64` |

The default is part of the language contract and is independent of platform,
target, backend, optimization level, and source order. It does not synthesize a
missing RHS or a default value; it only chooses the storage type for an existing
exact expression.

Defaulting never occurs inside a generalizable callable scheme, never fixes a
callable parameter or result from first/future use, and never replaces a closed
literal/`Add` constraint. If the exact value cannot materialize to the default
type, the author must provide an explicit wider admitted type. The compiler
must not silently select `i128` or another type based on literal magnitude.

## 8. Rule 7: arithmetic result and completion facts share the table

Every signed-integer `Add` row uses checked arithmetic. An upper or lower range
violation produces the nominal completion family `overflow`. It does not wrap,
invoke undefined behavior, depend on debug/release mode, or become an implicit
trap outside the operation-completion model.

The prelude declares exactly one `overflow` completion-family identity for this
purpose. It has no payload. Consequently:

```styio
overflow => recovery          // valid settlement arm
overflow(binding) => recovery // invalid: this family has no payload
```

`overflow` is an ordinary resolved identifier and does not enter the keyword
table. Integer `Add` has success type `T` and admitted completion bound
`{overflow}`.

Floating `Add` has an empty completion bound and uses strict IEEE 754 behavior:
round-to-nearest, ties-to-even; gradual underflow; preservation of subnormal
values and signed zero; and no dependence on the host floating-point rounding
environment. Fast-math, FTZ/DAZ, reassociation, or other transformations may not
change the Styio result. Infinity and NaN are permitted floating results and do
not produce `overflow`. This rule does not decide NaN payload bits,
canonicalization, equality, or ordering, but no optimizer may assume NaN or
infinity is absent.

## 9. Rule 8: compile-time and runtime use the same relation

Parsing, Sema, constant evaluation, typed lowering, runtime helpers, IDE type
display, and instance caches must agree on the same materialization and `Add`
rows. Constant folding cannot produce a value unavailable to the corresponding
runtime row, change rounding, hide integer `overflow`, or exploit a host-only
implicit conversion.

Diagnostics distinguish at least these obligations:

- an exact literal cannot materialize to its selected target;
- two concrete operands do not select one closed row; and
- integer addition takes the `overflow` completion edge.

The first two are definition/expression type errors. The third is an ordinary
operation completion and is governed by the existing settlement-or-propagation
rules.

## 10. Rule 9: generalized `Add` uses a finite conservative completion union

When a principal scheme retains a closed `Add` constraint whose legal concrete
rows have different completion sets, the scheme's stable completion upper bound
is the finite union of all those rows. The integer rows contribute
`{overflow}`; the floating rows contribute `{}`; therefore the union is
`{overflow}`.

For example, the accepted definition:

```styio
# add_five := (x) => x + 5
```

Its canonical internal relation can be displayed as:

```text
add_five : forall T. Fn(T) -> OperationSummary(T, {overflow})
           where Add(T, IntegerLiteral(5), T, Completion(T))
```

The scheme's parameter and result are the same admitted
scalar type, whose constraint requires the exact integer `5` to materialize to
that type, and whose completion upper bound is `{overflow}`. A particular
`f64` instantiation does not silently shrink the caller's obligation in this
language version. Instance-dependent completion-row narrowing remains behind
`F02`.

## 11. Rule 10: statically known overflow takes the same completion edge

If constant evaluation proves that a selected integer row overflows, it emits
the same `overflow` control edge that the runtime checked operation would emit.
Compile-time knowledge does not create a separate unconditional syntax-error
rule. An enclosing callable or settlement must admit, propagate, or settle that
completion under the existing static completion contract; only failure of that
existing obligation is an error.

This is distinct from materialization. In an `i8` context, `127 + 1` selects the
`i8` row and completes with `overflow`, while the single literal `128` cannot
materialize as `i8` and is a static materialization error.

## 12. Compiler resource budgets are implementation constraints

Implementations may use compact coefficient/exponent or arbitrary-precision
integer representations and may impose deterministic limits on token length,
significant digits, exponent magnitude, constant-evaluation work, and compiler
memory. The algorithms and numeric thresholds are implementation/plan choices,
not language values, types, promotions, or `Add` rows.

Exceeding such a budget produces a distinct compiler-resource diagnostic. It
must not truncate the literal, silently select a different type, round early,
wrap, or report the arithmetic completion family `overflow`. Within the
supported resource budget, the exact semantics in this document remain
normative.

## 13. Explicitly deferred and excluded

This decision does not silently freeze any of the following:

- literal suffixes or additional radix, separator, or exponent source
  spellings not already admitted by the EBNF;
- string concatenation or text `+` (`Q06`);
- container or matrix arithmetic/coercion rows (later `Q05` together with
  `Q08` where collection semantics are involved);
- unsigned integers, platform-width aliases, or additional scalar widths
  (later `Q05`);
- explicit conversion spelling or the narrowing, widening, saturation, and
  checked-conversion matrix (later `Q05`; no `cast[T]` spelling is presumed);
- subtraction, multiplication, division, division by zero, remainder, power,
  or wrapping operators (separate later `Q05` relations);
- NaN payload/canonicalization, equality, comparison, or total ordering (later
  `Q05`);
- operand dependency/order, completion-stop, and publication timing, now owned
  by accepted [Q03-F Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md);
- author-declared `Add`, user operator instances, overload priority, alternate
  result types, or instance-dependent completion rows (`F02`); or
- internal type-term representation, constraint indexes, cache layout,
  monomorphization keys, constant-evaluator algorithms, and concrete compiler
  resource thresholds (implementation plans).
