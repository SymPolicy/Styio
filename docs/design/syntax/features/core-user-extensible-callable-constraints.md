# User-Extensible Callable Constraints

**Purpose:** Record the decision to keep callable constraints compiler-owned until nominal ownership and coherent instance placement can make user extension deterministic.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.user-extensible-callable-constraints"
title = "User-Extensible Callable Constraints"
kind = "constraint-coherence"
decision_state = "deferred"
delivery_state = "not_started"
owner = "Sema / Modules"
syntax = "No constraint, trait, instance, implementation, priority, or orphan keyword/form is reserved."
resolution = "Keep numeric, comparable, indexable, iterable, and cloneable compiler-owned; if reopened, consider only owner-local non-overlapping instances and reject orphan, overlap, priority, and import-order resolution."
reopen_when = "Reopen only after nominal type ownership and callable member contracts have accepted/converged SSOTs and the owner resolves queued questions Q6.1-Q6.3 covering concrete instance heads, constraint-or-head ownership, and named static interface evidence."
golden_cases = []

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioSema/TypeInfer.cpp", "src/StyioSema/CallableInterface.cpp"]
evidence = ["tests/features/callable_constraints/t01_numeric_instances.styio"]

[prerequisites]
language-owner-approval = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_constraints/t01_numeric_instances.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"
module-contract = "docs/design/syntax/features/core-callable-interface-scheme-publication.md"

[implementation]
owner = "Sema / Modules"

[dependencies]
requires = [
  { id = "core.constrained-callable-relations", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.constrained-callable-relations"]
conflicts = []
supersedes = []
after = ["core.callable-interface-scheme-publication"]
```

## Decision

Q6-A was approved on 2026-07-31: open user instances remain deferred. The
closed compiler vocabulary continues to be resolved without dictionaries,
ambient imports, priorities, or user-authored implementation selection.

If the feature is reopened, owner-local non-overlapping instances are the only
reserved direction: an instance must live with the constraint or the nominal
type, orphan instances are invalid, and overlap is rejected deterministically.
This direction is not active syntax.

## Diagnostic and Compatibility Boundary

Current unsupported operator/capability cases continue to report the closed
compiler constraint that failed. No diagnostic should suggest a user instance
declaration that does not exist.

Adding an import must not change which callable constraint implementation is
selected. Existing module interfaces therefore remain independent of an open
instance environment.

## Evolution Boundary

Associated types, specialization, negative instances, blanket instances,
priorities, orphan instances, witness-table ABI, and source constraint syntax
remain separate decisions even after the reopen floor is met.

## Researched Reopen Direction

The primary-source refresh and unanswered owner batch live in
[Callable Type Evolution Questions](../CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md#queued-follow-up-question-set).
Rust's coherence rules and evolution record show why orphan and overlapping
instances consume upstream API freedom. GHC's orphan discovery and instance
resolution show the separate-compilation and termination cost of ambient or
recursive evidence. Swift's retroactive-conformance record shows that a
foreign/foreign conformance can leave an indeterminate runtime winner before
clients rebuild.

If the missing nominal-type prerequisites eventually converge, the researched
Styio-specific first slice is:

1. admit only one user constraint over one fully applied nominal concrete head;
2. allow the instance only where the constraint or outermost nominal head type
   is declared;
3. index coherence by the canonical
   `(constraint-id, nominal-type-id)` pair and reject duplicate, orphan,
   overlapping, priority, blanket, conditional, associated-type, and negative
   evidence;
4. give the instance a stable named identity and implementation digest in its
   owning `.styioi`;
5. resolve the instance before specialization, direct-call its concrete
   operations, and include its identity and digest in dependent interface and
   native-cache identities rather than passing a runtime witness table.

These points are recommendations only until Q6.1–Q6.3 are answered. The
feature remains deferred, and no constraint or instance source spelling is
reserved.
