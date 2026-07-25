# Styio Callable Principal Inference

**Purpose:** Define the accepted `Q02-INF` rule for definition-site principal inference of eligible callable bindings, including generalization, instantiation, rebinding, diagnostics, and later type-system boundaries.

**Last updated:** 2026-07-20

**Status:** Accepted owner decision `Q02-INF` on 2026-07-19.

**See also:** [Styio Language Design](./Styio-Language-Design.md) section 5.1,
[Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md)
section 1, [Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md),
and [Styio Language Decision Ledger](./Styio-Language-Decision-Ledger.md).

## 1. Scope and authority

This document is the sole detailed semantic owner for compiler-internal
callable principal inference. The language design, grammar, symbol reference,
and active-syntax map contain only summary mirrors of this decision.

`Q02-INF` applies to omitted callable type information in an eligible binding.
It does not add author-written generics, constraints, traits, overloads, or a
runtime type-class dictionary. It also does not change the accepted callable
source grammar.

The compiler may describe an inferred scheme with notation such as:

```text
forall A1, ... An. Fn(P1, ... Pm) -> OperationSummary(R, C) where K
```

This is specification and diagnostic metalanguage, not Styio source.
`forall`, `Fn`, `OperationSummary`, `Add`, `Literal`, type-variable names, and
constraint braces in such displays are not keywords, traits, tokens, or EBNF
productions. No source token, parser production, or author-visible declaration
form is introduced by `Q02-INF`.

An inferred callable scheme consists of:

- an ordered parameter-type vector `P1 ... Pm`;
- one success type `R`;
- one concrete finite completion-family upper bound `C`;
- a finite normalized set `K` of compiler-known closed constraints; and
- zero or more type variables quantified only at the scheme's outermost level.

Outermost-only quantification makes this rank-1 inference. The completion
component is never a quantified row, and constraints can refer only to the
compiler's closed built-in relation catalog.

## 2. Eligibility and explicit boundaries

Automatic rank-1 generalization is permitted only when every condition below
holds:

1. the binding is lexical-local or module-private;
2. it is a final callable binding formed with `# name := callable_value`;
3. the right side is a callable value, not an arbitrary value later discovered
   to be callable;
4. the definition is non-recursive, including absence from a recursive
   strongly connected component;
5. it is not a public/exported, native/FFI, or typed protocol boundary; and
6. capture analysis proves the callable value capture-safe.

Eligibility fails closed. In particular, lack of a capture-safety proof is not
permission to generalize. `Q04` owns the capture, borrowing, transfer, and
escape rules that establish that proof.

`Q10` owns the source spelling and resolution of visibility. `Q02-INF` only
consumes the resolved fact that a definition is lexical-local or module-private
and is not an exported boundary; it introduces no privacy/export modifier.

An eligible definition may omit parameter annotations. If it omits the entire
`: T` callable contract, the compiler infers both the success type and the
finite completion bound. If the author writes `: T`, that result type is an
immediate expected constraint and the missing completion clause still means an
empty bound in every scope; it never requests hidden completion inference.

Public/exported, recursive, native/FFI, and typed protocol-boundary callables
must source-declare every parameter type, the success type, and the finite
completion upper bound. A pure boundary writes `: T`, whose bound is empty. A
boundary admitting completions writes `: T ?| {family, ...}`. An internal
inferred scheme must never be published as an invisible public ABI.

## 3. Definition-site constraint solving

Inference reads exactly the definition, its lexical environment `Gamma`, the
closed built-in relation catalog, and any immediately written expected
contract. It does not read callers.

The compiler assigns fresh internal type variables to omitted parameter and
result positions. Name flow and result flow produce equality constraints;
literals and built-in operations produce closed representability and operation
constraints; operation analysis produces the completion facts. It then
unifies, simplifies, and canonically orders those facts at the definition site.
It must not first choose `i64`, `f64`, `any`, `dynamic`, an implicit top type,
or a backend-preferred type.

Inference succeeds only if the normalized facts have one principal scheme:
the scheme is most general, unambiguous, internally consistent, and
instantiable, and it is unique modulo alpha-renaming and canonical constraint
ordering. Incomparable candidate schemes, variables that cannot be determined
through the callable interface and admitted closed relations, inconsistent
constraints, or the absence of any instance are definition errors. The author
must then add explicit type information; the compiler does not guess.

After solving, the compiler generalizes exactly the eligible type variables
that are not free in `Gamma`. In metalanguage:

```text
quantified = free(P1 ... Pm, R, K) - free(Gamma)
```

Variables free in the lexical environment remain tied to that environment and
are not refreshed by use of the callable. `C` is already a concrete finite set
at this point. If the closed constraints cannot yield one stable finite bound,
the definition is rejected rather than generalized over a completion row.

The resulting parameter relation, result relation, normalized constraints, and
completion bound are stable facts of the binding. Later uses cannot narrow,
widen, or otherwise rewrite the scheme.

## 4. Required examples

The following definition is valid:

```styio
# identity := (x) => x
```

Its inferred fact can be displayed in metalanguage as:

```text
identity : forall A. Fn(A) -> OperationSummary(A, {})
```

The same quantified variable in the parameter and success positions preserves
the input type. No call site chooses and stores one permanent concrete type.

This definition is also valid:

```styio
# add_five := (x) => x + 5
```

It preserves an operation constraint rather than defaulting `x`. An
illustrative internal display is:

```text
add_five : forall T. Fn(T) -> OperationSummary(T, {overflow})
           where Add(T, IntegerLiteral(5), T, Completion(T))
```

Accepted decision `Q05-LIT-ADD`, owned by
[Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md),
now closes that relation. `T` ranges over the finite admitted scalar rows, the
integer literal remains exact until it materializes to `T`, and the selected row
returns `T`. Integer rows admit `{overflow}` and floating rows admit `{}`, so the
stable generalized scheme uses their conservative union `{overflow}`. `Add` and
`IntegerLiteral` are not author-writable capabilities. `Completion(T)` is the
closed relation's finite projection, not a completion-row variable, and no
backend-preferred integer type determines the scheme.

## 5. Fresh instantiation

Every call or other permitted use independently instantiates all outermost
quantified variables with fresh variables, applies the normalized constraints,
and then solves for that use's concrete types. For example,
`identity(1)` cannot affect a later `identity("s")` call. Non-quantified
variables tied to the lexical environment are not refreshed.

A concrete argument that cannot satisfy an instantiated constraint is a
call-site error. The definition body is not re-type-checked as an unconstrained
C++-template-style body for each use; it was already validated symbolically at
the definition site.

The scheme and its instances are independent of first use, future/downstream
calls, whole-program caller scans, file order, hash iteration order, and
optimization order. There is no first-use fixation and no future-call-site
back-inference. Canonical schemes and normalized concrete type vectors provide
stable cache and monomorphization keys.

## 6. Mutable callable rebinding

A bare mutable form `# f = new_body` never creates, generalizes, weakens,
narrows, widens, or defaults a callable scheme. It is legal only when `f`
already has an established expected scheme. The new body is checked against
that complete parameter, result, constraint, and completion contract.

An initial mutable callable therefore needs a complete explicit source
contract, including all parameter types and the success/completion contract, or
some other already established expected scheme. In the current source surface,
author-written generic schemes are not available, so an ordinary explicit
initial mutable definition is monomorphic. No weak variable may remain for a
later call to fix.

A rebinding implementation may have a smaller actual completion set than the
existing upper bound, but the visible bound and the rest of the scheme remain
unchanged. An implementation that exceeds the bound or fails any existing
relation is rejected at the rebinding site. Changing the scheme requires a new
name. A final `:=` binding cannot be rebound.

## 7. Diagnostics and failure-closed behavior

The diagnostic site follows the failed obligation:

- an ineligible omitted contract, unsafe or unproved capture, recursive
  omission, missing principal scheme, ambiguity, inconsistent closed
  constraints, or absence of one concrete finite completion bound is reported
  at the definition;
- failure of concrete arguments, literal representability, or a built-in
  relation after fresh instantiation is reported at that call;
- failure to implement an established scheme is reported at the rebinding; and
- a public/exported, native/FFI, recursive, or typed protocol boundary missing
  explicit parameter/result/completion facts is reported at that boundary.

Diagnostics may show the canonical scheme and the origins of constraints, but
must label `forall`, `Add`, `Literal`, and similar displays as inferred
metalanguage rather than text written by the author.

Failure never injects `any`, `dynamic`, an implicit top type, a numeric default,
a backend default, an unknown placeholder signature, or a "compile now, decide
later" type. Default parameters, variadics, name-based overload sets, user
operator instances, and structural duck typing are not escape hatches for
`Q02-INF`.

All quantified variables and closed constraints must be solved for a concrete
instance before typed lowering. Unresolved type variables do not enter SGIR or
LLVM, and this decision introduces no runtime generic dictionary.

## 8. Completion bounds

The completion component follows `Q02-BC` and `Q02-SIG`: it is a static finite
upper bound, not a returned union, runtime exception set, or open effect row.
For an omitted eligible local/private contract, definition analysis infers the
whole operation summary. For a written contract, the implementation's actual
set must remain a subset of the written upper bound.

Accepted `Q05-LIT-ADD` supplies the concrete example of this rule: its closed
integer rows contribute `{overflow}`, its floating rows contribute `{}`, and a
scheme spanning both uses the finite conservative union `{overflow}`. A
particular floating instantiation does not shrink that visible scheme. Other
built-in relations must likewise receive a separately accepted finite bound or
remain invalid; `Q02-INF` never introduces a completion-row variable.

## 9. Ownership boundaries

- **`Q04`:** defines capture safety, borrowing, ownership, transfer, and escape.
  `Q02-INF` consumes only a proven capture-safe result and otherwise rejects
  implicit generalization.
- **`Q05-LIT-ADD`:**
  [Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md)
  defines exact integer/decimal materialization, the closed scalar `+`
  operand/result rows, late concrete defaults, integer `overflow`, strict
  floating behavior, and the finite generalized completion union. Other
  operators, explicit conversions, and remaining NaN policy stay with later
  `Q05`; `Q02-INF` only preserves and solves accepted closed facts.
- **`F02`:** owns any future author-written quantification or constraints,
  public generic contracts, user-defined capability/operator instances and
  their coherence, higher-order polymorphism, higher-rank polymorphism, and
  completion-row variables. None is admitted by `Q02-INF`.
- **`Q10`:** owns visibility syntax and name/export resolution. `Q02-INF`
  consumes its resolved local/private-versus-boundary fact and adds no new
  visibility spelling.
