# Context-Driven Call Instantiation

**Purpose:** Own fresh callable-scheme instantiation from ordinary arguments and concrete surrounding expectations, including fail-closed behavior for underconstrained calls.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.context-driven-call-instantiation"
title = "Context-Driven Call Instantiation"
kind = "callable-type-semantics"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / Type System"
syntax = "identity(value) with an optional concrete expected type supplied by the enclosing expression"
resolution = "Instantiate inferred relations from ordinary argument types and the enclosing concrete expected type; reject unresolved variables and never parse authored call-site type arguments."
golden_cases = ["tests/features/inferred_generics/t02_context_empty_list.styio", "tests/features/inferred_generics/t06_scalar_expected_type.styio", "tests/features/inferred_generics/t07_function_context.styio", "tests/features/inferred_generics/e01_underconstrained_result.styio", "tests/features/inferred_generics/e03_authored_generic_binder.styio", "tests/features/inferred_generics/e04_call_site_type_arguments.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioAST/AST.hpp", "src/StyioSema/TypeInfer.cpp", "src/StyioSema/SemaContext.hpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/inferred_generics/t02_context_empty_list.styio", "tests/features/inferred_generics/t06_scalar_expected_type.styio", "tests/features/inferred_generics/t07_function_context.styio", "tests/features/inferred_generics/e01_underconstrained_result.styio", "tests/features/inferred_generics/e03_authored_generic_binder.styio", "tests/features/inferred_generics/e04_call_site_type_arguments.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/inferred_generics/t02_context_empty_list.styio"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "instantiate_callable_type_scheme"
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = ["core.recursive-callable-group"]
```

## Decision

Every generic callable use receives fresh relation variables. Ordinary
arguments constrain parameter-side variables, and an enclosing concrete
expectation may constrain result-only variables:

```styio
# make_empty := () => []
numbers : list[i64] := make_empty()
words : list[string] := make_empty()
```

If any variable required for a concrete runtime instance remains unresolved,
the call is underconstrained and Sema rejects it. The diagnostic asks for a
concrete surrounding annotation.

## Source-Syntax Boundary

Authors never supply callable type arguments. `# identity[T] ...` is not a
callable declaration, and `identity[i64](1)` remains ordinary value indexing
followed by a call attempt. Type application with `[]` exists only inside
type-position forms such as `list[i64]`.

## Functional-Language Boundary

Expected-type propagation is bidirectional but local: it constrains the
current pure expression without making specialization order observable. Every
use is checked independently, preserving referential transparency across
instances.

## Lowering Boundary

A completely resolved relation produces a deterministic specialization key and
symbol. Equal concrete relations deduplicate to one emitted body; unresolved
or conflicting relations never reach lowering.

## Diagnostic Boundary

Underconstrained calls, relation conflicts, authored binders, and value-position
indexing errors are distinct failures. No diagnostic may recommend syntax that
the language does not own.

## Compatibility Boundary

`[]` keeps its existing selector meaning in value position. Concrete
monomorphic calls continue through the existing call checker and symbol.

## Evolution Boundary

Defaulting, overload resolution, implicit conversions inside relation
variables, specialization budgets, and interface-visible instance selection
require separate feature decisions.
