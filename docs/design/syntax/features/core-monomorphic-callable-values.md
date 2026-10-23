# Monomorphic Callable Values

**Purpose:** Own the first executable first-class callable-value boundary: a final, noncapturing named callable item contextually coerced to one concrete monomorphic callable type.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.monomorphic-callable-values"
title = "Monomorphic Callable Values"
kind = "callable-value-semantics"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / IR"
syntax = "In type position, `#(T1, T2): R`; in value position, a bare final noncapturing callable name may coerce only under one complete concrete callable type."
resolution = "Represent each named callable or concrete inferred instance as a distinct compiler-owned function item and coerce it contextually to an allocation-free monomorphic callable value; do not admit captures, generalized storage, address equality, or implicit signature guessing."
golden_cases = [
  "tests/features/callable_values/t02_monomorphic_callable_value.styio",
  "tests/features/callable_values/t03_contextual_generic_callable_values.styio",
  "tests/features/callable_values/t04_pass_and_return_callable_values.styio",
  "tests/features/callable_values/e05_callable_signature_mismatch.styio",
  "tests/features/callable_values/e06_mutable_callable_slot.styio",
  "tests/features/callable_values/e07_capturing_callable_item.styio",
  "tests/features/callable_values/e08_non_callable_value.styio",
  "tests/features/callable_values/e09_callable_container.styio",
]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioToken/Token.hpp", "src/StyioParser/Parser.cpp", "src/StyioAST/AST.hpp", "src/StyioSema/TypeInfer.cpp", "src/StyioIR/GenIR/SGIR.hpp", "src/StyioLowering/AstToStyioIR.cpp", "src/StyioCodeGen/GetTypeG.cpp", "src/StyioCodeGen/CodeGenG.cpp"]
evidence = ["tests/features/callable_values/t02_monomorphic_callable_value.styio", "tests/features/callable_values/t03_contextual_generic_callable_values.styio", "tests/features/callable_values/t04_pass_and_return_callable_values.styio", "tests/features/callable_values/e05_callable_signature_mismatch.styio", "tests/features/callable_values/e06_mutable_callable_slot.styio", "tests/features/callable_values/e07_capturing_callable_item.styio", "tests/features/callable_values/e08_non_callable_value.styio", "tests/features/callable_values/e09_callable_container.styio"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/README.md"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
typed-ir-boundary = "docs/rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "StyioSemaContext::typeInfer(NameAST* ast)"
owner = "Sema / IR"

[dependencies]
requires = [
  { id = "core.higher-order-callable-polymorphism", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.callable-specialization-policy", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.higher-order-callable-polymorphism"]
conflicts = []
supersedes = []
after = []
```

## Decision

Q1-A was approved on 2026-07-31. A final named callable with no captured
environment has a distinct compiler-owned item identity. A concrete callable
value exists only after that item is checked against one complete monomorphic
type:

```styio
# increment : i64 := (value: i64) => value + 1
operation: #(i64): i64 := increment
```

`#(i64): i64` is type-position syntax. It does not declare a callable, capture
an environment, or introduce a generic binder. A generic inferred callable may
coerce only when the expected callable type determines every relation
variable; the resulting concrete specialization is the item being coerced.

## Semantic Boundary

The item itself is compiler metadata and requires no allocation. Crossing a
runtime value boundary produces one typed noncapturing entry value. Calls
through that value use the declared parameter/result ABI and preserve the
callee's effects. Finality is required because rebinding would make item
identity time-dependent.

Callable types are invariant in this first slice. There is no parameter
contravariance, result covariance, effect subtyping, variadic callable value,
optional parameter adaptation, or implicit numeric signature conversion.

## Diagnostic and Compatibility Boundary

A missing context reports that the callable item needs a concrete callable
type. A mismatched arity or parameter/result type reports both canonical
signatures before lowering. A capturing or mutable callable reports that Q1
admits final noncapturing items only. A list, dictionary, or topology type
containing a callable is rejected before lowering because callable-container
representation remains a separate decision.

Existing direct named calls keep their current source meaning and symbol
identity. No ordinary value, raw integer, native address, or generalized
scheme is implicitly reinterpreted as a callable value.

## Evolution Boundary

Captured environments belong to
[Affine Capturing Closures](./core-affine-capturing-closures.md). Rank-2
callbacks belong to
[Rank-2 Callback Polymorphism](./core-rank2-callback-polymorphism.md).
Callable address equality, native pointer interop, dynamic loading, and
impredicative callable containers require separate decisions.

## Delivered Contract

The parser canonicalizes callable types without whitespace as
`#(T1,T2):R`. Sema accepts only exact invariant signatures, freezes an inferred
principal relation from the complete expected signature, and rejects mutable,
capturing, non-callable, or mismatched values before lowering.

StyioIR represents the frozen value as an allocation-free function reference
and marks calls through a bound callable as indirect calls carrying their full
canonical signature. LLVM lowering maps callable values to function pointers;
they never enter the string ownership or cleanup path.

Acceptance evidence covers concrete and inferred callable items, binding,
passing, returning, indirect execution, nested signature parsing,
callable-container rejection, and each fail-closed boundary. The focused
command is:

```bash
ctest --test-dir build -L callable_values --output-on-failure --no-tests=error
```
