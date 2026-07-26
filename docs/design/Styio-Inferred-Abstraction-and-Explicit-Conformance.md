# Styio Inferred Abstraction and Explicit Conformance

**Purpose:** Record the accepted portion of `F1-ABSTRACTION`: inferred
rank-1 polymorphism and inferred capability requirements with explicit,
coherent concrete conformance.

**Last updated:** 2026-07-26

**Status:** Partially accepted owner decision `F1-INFERRED-ABSTRACTION`.

## 1. No authored generic parameter list

Styio does not admit an authored `[T]`, `[Item: type]`, `forall`, or equivalent
generic-parameter declaration list.

```styio
# identity := (value) => value
# pair := (left, right) => (left, right)
```

When a definition has a unique principal constrained rank-1 scheme, the
compiler quantifies the necessary type variables internally. These variables
are specification/compiler facts, not source names.

Inference occurs at the definition. First use, call order, import order,
future calls, backend declarations, and elapsed compilation order cannot fix
or mutate the scheme. Every call freshly instantiates it.

## 2. Publication and failure-closed boundary

A private or public final callable may publish an inferred contract only when
the compiler derives one unique, stable principal type and finite completion
upper bound from the definition and its lexical environment. The canonical
contract is stored in the module interface so dependants do not reanalyse the
body.

Definitions that are ambiguous, inference-recursive without a stable finite
solution, or over deterministic constraint/specialization budgets fail at the
definition. The compiler never uses a first-call monotype or backend fallback.

Native/FFI and resource-protocol ABI boundaries remain concrete. Other
boundaries may require explicit concrete information when principal inference
cannot prove a stable public contract.

## 3. Capability requirements and conformance

Operations used by a callable body generate its required capability relations.
The author does not repeat inferred `T: Iterable + Eq + Hash`-style clauses.

Built-in types receive capabilities from the closed compiler/prelude catalog.
A user type satisfies a protocol only through an explicit coherent
implementation. Matching member names do not create conformance.

Unknown, ambiguous, missing, overlapping, or order-dependent implementations
fail statically. Tooling may propose an implementation scaffold but cannot
silently install or assume one.

## 4. Runtime and deferred surface

Accepted inferred abstractions are rank-1 and use deterministic
monomorphization. Each canonical concrete key produces at most one instance;
unused instances are not emitted. There is no runtime generic dictionary,
reflection fallback, higher-rank value, higher-kinded type, specialization, or
polymorphic recursion.

This decision does not yet admit user types into operator rows, user-defined
conversions, associated types, dynamic dispatch, or new operator glyphs. Those
remain unresolved parts of `F1-ABSTRACTION`.
