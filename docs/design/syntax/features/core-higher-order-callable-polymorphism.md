# Higher-Order Callable Polymorphism

**Purpose:** Decide whether a generalized callable may remain polymorphic when passed, stored, captured, or returned as a first-class value.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.higher-order-callable-polymorphism"
title = "Higher-Order Callable Polymorphism"
kind = "callable-value-semantics"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / IR"
syntax = "No new source form selected; existing callable/value positions require an explicit semantic boundary."
resolution = "Keep inferred schemes available only to direct named calls; any passed, stored, captured, or returned callable value must freeze to one concrete monomorphic function type."
golden_cases = ["tests/features/callable_values/t01_direct_named_instances.styio", "tests/features/callable_values/e04_captured_scheme.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]
implementation = ["src/StyioSema/TypeInfer.cpp"]
evidence = ["tests/features/callable_values/t01_direct_named_instances.styio", "tests/features/callable_values/e01_stored_scheme.styio", "tests/features/callable_values/e02_passed_scheme.styio", "tests/features/callable_values/e03_returned_scheme.styio", "tests/features/callable_values/e04_captured_scheme.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_values/t01_direct_named_instances.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"
ir-contract = "docs/rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "validate_generalized_callable_value_positions"
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.effect-aware-callable-generalization", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.constrained-callable-relations", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = ["core.effect-aware-callable-generalization", "core.constrained-callable-relations"]
```

## Decision

Inferred schemes are available only at direct named call sites. Passing,
storing, capturing, or returning a callable first requires one concrete
monomorphic function type supplied by the surrounding context. A capturing
local callable is not generalized.

Generalized callable values may be reconsidered only after typed callable IR,
closure-environment ownership, effect summaries, and a deliberate
bidirectional higher-rank checking rule exist.

## Implemented Boundary

Sema validates every generalized callable reference after principal schemes
have been prepared. A scheme name used as an ordinary value in a binding,
argument, return, collection, task capture, or other traversed value position
is rejected before lowering. The direct call node keeps the callee name outside
the value-expression path, so ordinary `identity(value)` instantiation remains
available.

The current grammar and StyioIR do not expose a concrete callable-value type
boundary. Consequently, this convergence proves the fail-closed half of the
decision: a generalized scheme cannot escape and cannot accidentally degrade
to an untyped pointer or backend placeholder. It does not establish executable
monomorphic closures or higher-rank values.

## Diagnostic Boundary

The stable diagnostic identifies the inferred callable, says that schemes are
limited to direct named calls, and identifies the missing concrete monomorphic
callable-value boundary. It does not suggest authored `forall`, callable type
arguments, or a source form that the grammar does not provide.

The `callable_values` goldens pair successful direct integer/string instances
with stored, passed, returned, and task-captured scheme escapes.

## Evolution Boundary

Rank-2 callbacks, impredicative containers, polymorphic fields, and generalized
capturing closures require separate child decisions.
