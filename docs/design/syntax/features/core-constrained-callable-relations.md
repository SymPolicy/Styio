# Constrained Callable Relations

**Purpose:** Decide how inferred callable variables record requirements introduced by operators, comparisons, collection operations, and capabilities that plain equality unification cannot express.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.constrained-callable-relations"
title = "Constrained Callable Relations"
kind = "callable-type-semantics"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / Type System"
syntax = "No source constraint syntax; constraints are inferred from ordinary expressions."
resolution = "Infer a closed compiler-owned constraint vocabulary from ordinary expressions; do not add user-defined traits, instances, or source constraint syntax."
golden_cases = ["tests/features/callable_constraints/t01_numeric_instances.styio", "tests/features/callable_constraints/t03_indexable_instances.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Handle-Capability-Type-System.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]
implementation = ["src/StyioSema/TypeInfer.cpp", "src/StyioSema/SemaContext.hpp", "src/StyioLowering/AstToStyioIR.cpp", "src/StyioCodeGen/CodeGenG.cpp"]
evidence = ["tests/features/callable_constraints/t01_numeric_instances.styio", "tests/features/callable_constraints/t02_comparable_instances.styio", "tests/features/callable_constraints/t03_indexable_instances.styio", "tests/features/callable_constraints/t04_transitive_numeric_constraint.styio", "tests/features/callable_constraints/e01_numeric_constraint_mismatch.styio", "tests/features/callable_constraints/e02_comparable_constraint_mismatch.styio", "tests/features/callable_constraints/e03_indexable_constraint_mismatch.styio"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/design/Styio-EBNF.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_constraints/t01_numeric_instances.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
capability-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "prepare_callable_type_schemes"
owner = "Sema / Type System"

[dependencies]
requires = [
  { id = "core.inferred-callable-relation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.type-expression", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.inferred-callable-relation"]
conflicts = []
supersedes = []
after = []
```

## Decision

```styio
# double := (value) => value + value
# first := (items) => items[0]
```

The compiler owns a closed, normalized constraint vocabulary derived from
existing type and capability facts: numeric, comparable, indexable, iterable,
and cloneable. Constraints remain semantic metadata checked at instantiation.
There are no user-defined instances, orphan rules, authored constraint clauses,
or source-level trait declarations in this feature.

## Prerequisite Boundary

Sema records `numeric`, `comparable`, and `indexable` constraints from
arithmetic, comparison, and index expressions in the current principal-relation
subset. Constraints propagated through a called inferred scheme are
re-instantiated with fresh variables, reduced to a deterministic canonical
order, and solved to a fixed point at each concrete use. Structural list and
dictionary facts determine index and result types; unsatisfied constraints fail
before specialization or lowering.

`iterable` and `cloneable` are part of the closed compiler vocabulary and use
the same capability predicates, but the current principal-relation expression
subset has no pure source expression that emits either constraint. Their owning
iterator or clone expression features must add source evidence before treating
those emitters as active.

Comparison lowering now carries operand types into StyioIR. String equality and
ordering therefore use lexical string values, while integer, floating-point,
boolean, and character comparisons retain their scalar lowering paths. Numeric
conversion and literal defaulting remain owned by the dependent
`core.ambiguous-literal-defaulting` feature.

The `callable_constraints` goldens prove independent integer/floating numeric
instances, scalar and string comparisons, list and dictionary indexing,
transitive constraint propagation, and stable negative diagnostics for each
currently emitted constraint family.

## Evolution Boundary

Open instances, orphan/coherence rules, associated types, specialization among
instances, and constraint syntax require child feature SSOTs.
