# Inferred Callable Relation

**Purpose:** Own definition-site inference and principal rank-1 generalization for eligible final callable bindings without source-level generic binders.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.inferred-callable-relation"
title = "Inferred Callable Relation"
kind = "callable-type-semantics"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / Type System"
syntax = "# identity := (value) => value"
resolution = "Infer one principal rank-1 relation at each eligible final callable definition, generalize only at its definition boundary, and keep all type variables compiler-owned."
golden_cases = ["tests/features/inferred_generics/t01_identity_multi_instance.styio", "tests/features/inferred_generics/t05_generic_composition.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioSema/TypeInfer.cpp", "src/StyioSema/SemaContext.hpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/inferred_generics/t01_identity_multi_instance.styio", "tests/features/inferred_generics/t05_generic_composition.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/inferred_generics/t01_identity_multi_instance.styio"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "prepare_callable_type_schemes"
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.callable-binding", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.keyword-free-lexical-contract", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.callable-binding"]
conflicts = []
supersedes = []
after = []
```

## Decision

An eligible final callable receives a compiler-owned type scheme derived from
its parameters, body, and concrete annotations. Unbound relation variables are
generalized exactly once at the definition boundary. Every later use
instantiates a fresh copy, so this program has two independent instances:

```styio
# identity := (value) => value
>_(identity(1))
>_(identity("hello"))
```

Authors do not name the inferred variables. A source annotation always names a
concrete or previously defined Styio type; it never implicitly declares a type
parameter.

## Semantic Boundary

Inference is rank-1 and equality-based for the currently supported pure scalar,
list, and dictionary relations. A body operation that cannot yield a principal
relation fails closed instead of guessing an overload or introducing a hidden
higher-rank constraint.

Only final callable bindings are generalized. Mutable callable bindings remain
monomorphic because rebinding would otherwise make the published scheme
unstable.

## Lowering Boundary

The scheme is semantic metadata, not a runtime value. Lowering records
deterministic, demand-driven concrete specializations and emits only the
instances reached by checked calls. Generated code does not allocate a generic
dictionary, box values, or introduce a garbage collector.

## Diagnostic Boundary

Diagnostics identify the callable and the relation site that cannot be unified.
They must not suggest authored generic binders or call-site type arguments.

## Compatibility Boundary

Fully annotated callables keep their existing monomorphic contract. Existing
unannotated final callables whose bodies already forced one concrete type
continue to infer that concrete relation.

## Evolution Boundary

Effects, overload constraints, higher-rank values, interface serialization,
and specialization budgets require separate feature decisions. They must
extend this SSOT instead of changing principal rank-1 inference implicitly.
