# Recursive Callable Group

**Purpose:** Own strongly connected callable-group inference, group-internal monomorphism, boundary generalization, and rejection of polymorphic recursion.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.recursive-callable-group"
title = "Recursive Callable Group"
kind = "callable-type-semantics"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / Type System"
syntax = "A self-recursive or mutually recursive final callable component inferred as one dependency group."
resolution = "Assign one provisional monotype to every SCC member, solve all internal edges against those monotypes, generalize at the group boundary, and reject any internal edge that needs a different instantiation."
golden_cases = ["tests/features/inferred_generics/t03_recursive_group_boundary.styio", "tests/features/inferred_generics/t04_mutual_recursive_scc.styio", "tests/features/inferred_generics/e02_polymorphic_recursion.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioSema/TypeInfer.cpp", "src/StyioSema/SemaContext.hpp", "src/StyioLowering/AstToStyioIR.cpp"]
evidence = ["tests/features/inferred_generics/t03_recursive_group_boundary.styio", "tests/features/inferred_generics/t04_mutual_recursive_scc.styio", "tests/features/inferred_generics/e02_polymorphic_recursion.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/inferred_generics/t04_mutual_recursive_scc.styio"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "prepare_callable_type_schemes"
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = []
```

## Decision

The callable dependency graph is partitioned into strongly connected
components. Each member of one component receives one provisional monotype,
and every recursive edge in that component must reuse it. After all member
bodies and outgoing dependencies are solved, the stable component types are
generalized at the common boundary.

This permits ordinary generic recursion: separate external calls may
instantiate the resulting scheme at different concrete types. It also permits
mutual recursion with the same rule and without depending on source order.

## Rejected Form

An internal recursive edge may not instantiate its target differently from the
target's provisional monotype. For example, a call that changes `T` into
`list[T]` on every recursive step is polymorphic recursion and is rejected.
Styio does not accept an annotation that opts into that behavior.

## Algorithm and Complexity Boundary

Sema builds the final-callable dependency graph once, computes SCCs with
Tarjan's linear-time algorithm, and solves the condensed DAG in dependency
order. It does not repeatedly rescan all definitions for each call site.

## Diagnostic Boundary

A conflicting group-internal edge reports polymorphic recursion and names the
callable relation context. It must not silently widen the group, clone a new
recursive instance, or defer failure to LLVM.

## Compatibility Boundary

Concrete recursive callables remain monomorphic and preserve their existing
lowering. Only eligible final callables with unresolved relation variables
publish a generalized group-boundary scheme.

## Evolution Boundary

Explicit polymorphic recursion, higher-rank recursion, effectful recursive
generalization, and cross-module SCCs require separate decisions and cannot be
introduced as compatibility behavior.
