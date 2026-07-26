# Styio Exact Numeric Literals

**Purpose:** Define the accepted exact integer/decimal literal terms,
failure-closed contextual materialization, late concrete defaults, constant
equivalence, and compiler resource boundary.

**Last updated:** 2026-07-26

**Status:** Accepted exact-literal portion of former `Q05-LIT-ADD`, retained
under `Q05-NUMERIC-OPS`. The former same-type Add table is superseded.

**See also:** [Styio Built-in Numeric Operators and
Inference](./Styio-Builtin-Numeric-Operators-and-Inference.md),
[Styio Checked Scalar Conversion](./Styio-Checked-Scalar-Conversion.md), and
[Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md).

## 1. Scope and authority

This document is the sole detailed semantic owner for:

- exact integer and decimal terms before storage materialization;
- contextual materialization to `i8` through `i128`, `u8` through `u128`,
  `f32`, or `f64`;
- signed decimal zero;
- late `i64`/`f64` defaults; and
- deterministic compiler resource failures for literal processing.

Built-in operator rows, concrete operand/result inference, implicit widening,
comparison, completion sets, and compound assignment are solely owned by
[Styio Built-in Numeric Operators and
Inference](./Styio-Builtin-Numeric-Operators-and-Inference.md).

`IntegerLiteral` and `DecimalLiteral` below are compiler/specification
metalanguage, not source type names or runtime arbitrary-precision values. No
keyword, suffix, token, or expression form is introduced here.

## 2. Literals remain exact before materialization

Before a concrete storage type is selected, an integer literal denotes its
arbitrary-precision signed mathematical integer. Positive and negative source
forms enter the same exact term, so integer `-0` and `0` denote the same value.

A decimal literal retains an exact decimal coefficient and base-ten exponent.
It also retains whether an explicitly signed decimal zero was negative so
source `-0.0` can materialize as IEEE negative zero.

Radix and separators, when independently admitted by the grammar, affect only
source representation. Definition-site generalization never chooses a storage
width for an exact literal.

The sign written as part of a negative exact literal belongs to this term.
After materialization, runtime unary `-` is the separately owned checked/IEEE
operator.

## 3. Contextual materialization

Materialization converts one exact term to a context-selected concrete type:

| Exact source term | Target | Accepted only when | Result |
|---|---|---|---|
| `IntegerLiteral(v)` | `i8`…`i128` or `u8`…`u128` | `v` is in the target range | exact target value |
| `IntegerLiteral(v)` | `f32` or `f64` | the target format represents `v` exactly | exact floating value |
| `DecimalLiteral(d)` | any fixed integer | never implicitly | materialization error |
| `DecimalLiteral(d)` | `f32` or `f64` | rounding the finite source does not produce infinity | round-to-nearest, ties-to-even |

Failure is a static materialization/type diagnostic. It never truncates,
wraps, saturates, silently chooses a wider type, changes literal kind, or
becomes a runtime conversion completion.

Decimal-to-floating materialization preserves gradual underflow, subnormal
values, and signed zero. Explicit decimal `-0.0` materializes as negative
zero; exact integer zero has no negative-zero identity.

## 4. Context and operator integration

Expected binding, argument, return, or operator types are applied before any
default. A numeric operator may use an exact literal only through an admitted
row whose selected concrete target satisfies section 3.

Materialization is symmetric with respect to a literal's source position:
when an admitted row selects `T`, either `T op Literal` or `Literal op T` may
materialize the literal to `T`.

Two exact terms may be simplified with exact arithmetic while constraints are
being solved. Such simplification remains provisional. Once an operator row
is selected, constant evaluation must be observationally identical to
materializing operands and executing that row. Exact folding cannot bypass a
materialization failure, selected result type, completion, or floating
rounding point.

The accepted operator catalog may infer a common result for two already
concrete operands. That concrete inference is not literal materialization and
is not owned by this document.

## 5. Late defaults

If an exact literal or all-literal expression remains unconstrained when an
ordinary storage binding, non-generic return, or other concrete value boundary
requires a type, Styio applies exactly one stable default:

| Exact term | Late default |
|---|---|
| integer | `i64` |
| decimal | `f64` |

The default is target-, backend-, optimization-, platform-, and source-order
independent. It chooses the storage type for an existing expression; it never
synthesizes a missing value.

Defaulting never occurs inside a generalizable callable scheme, never fixes a
parameter/result from first or future use, and never replaces a retained
closed literal/operator constraint. If a value cannot materialize to the late
default, the author must provide an admitted explicit context; the compiler
does not widen solely from literal magnitude.

## 6. Inference boundary

Q02 may retain exact literal terms in a closed operator constraint. Each use
receives fresh instantiation, and no storage type is chosen until the accepted
relation or an ordinary concrete boundary requires it.

For example, the internal display for `x + 5` may retain
`IntegerLiteral(5)` inside the built-in numeric relation. The literal itself
does not introduce an author-visible generic, runtime dictionary, open trait,
or completion-row variable.

## 7. Constant/runtime identity

Parsing, Sema, constant evaluation, typed lowering, runtime execution, IDE
display, and caches must agree on the exact term and its selected
materialization.

Constant folding cannot:

- choose a type unavailable to runtime row selection;
- round an integer literal before a target is selected;
- round a decimal through a host type first;
- hide a materialization diagnostic;
- change signed-zero identity; or
- turn an operator completion into a materialization error.

Diagnostics distinguish literal token/resource failure, materialization
failure, missing/ambiguous operator row, and operation completion.

## 8. Compiler resource budgets

Implementations may use compact coefficient/exponent and arbitrary-precision
integer representations and may impose deterministic limits on token length,
significant digits, exponent magnitude, constant-evaluation work, and compiler
memory.

Exceeding a budget produces a distinct compiler-resource diagnostic. It must
not truncate, round early, silently select another type, wrap, or report a
runtime arithmetic completion. Within the supported budget, the semantics
above are normative.

## 9. Excluded

This owner does not admit:

- new radix, separator, exponent, or suffix spelling absent from the EBNF;
- runtime big integer, decimal, rational, or arbitrary-precision types;
- platform-width scalar types;
- text, scalar/code-point, container, matrix, or user-defined materialization;
- intentionally lossy round/truncate/saturate/wrap operations; or
- author-declared literal/operator instances.
