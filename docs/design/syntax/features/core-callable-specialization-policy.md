# Callable Specialization Policy

**Purpose:** Decide reachability, ownership, caching, linkage, and growth controls for concrete callable instances.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.callable-specialization-policy"
title = "Callable Specialization Policy"
kind = "generic-codegen-policy"
decision_state = "accepted"
delivery_state = "converged"
owner = "Lowering / Codegen"
syntax = "No source-level explicit-instantiation syntax proposed."
resolution = "Collect only reachable instances, assign deterministic single-owner symbols, key reuse by canonical semantic and backend digests, and fail closed at recursive or pathological growth ceilings."
golden_cases = ["tests/features/callable_specialization/t01_reachable_instances.styio", "tests/features/callable_specialization/e01_instantiation_depth.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"]
implementation = ["src/StyioSema/CallableSpecializationGraph.cpp", "src/StyioSema/TypeInfer.cpp", "src/StyioLowering/AstToStyioIR.cpp", "src/main.cpp"]
evidence = ["tests/features/callable_specialization/t01_reachable_instances.styio", "tests/features/callable_specialization/t02_reordered_instances.styio", "tests/features/callable_specialization/d01_dependency_base.styio", "tests/features/callable_specialization/d02_dependency_changed.styio", "tests/features/callable_specialization/d03_imported_concrete_root.styio", "tests/features/callable_specialization/modules/reachable.styio", "tests/features/callable_specialization/e01_instantiation_depth.styio", "tests/run_callable_specialization_case.py"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/callable_specialization/t01_reachable_instances.styio"
research-basis = "docs/design/syntax/CALLABLE-TYPE-DECISION-AGENDA-2026-07-30.md"
backend-contract = "docs/specs/AGENT-SPEC.md"
current-semantic-contract = "docs/design/Styio-Language-Design.md"

[implementation]
path = "src/StyioSema/CallableSpecializationGraph.cpp"
symbol = "CallableSpecializationGraph::register_item"
owner = "Lowering / Codegen"

[dependencies]
requires = [
  { id = "core.context-driven-call-instantiation", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.callable-interface-scheme-publication", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.context-driven-call-instantiation"]
conflicts = []
supersedes = []
after = ["core.callable-interface-scheme-publication"]
```

## Decision

Normal builds collect a lazy reachable mono-item graph. Each instance receives
one deterministic owner and a content-addressed identity derived from its
canonical relation, portable semantic body, dependencies, target, and ABI facts. A hard
recursive-instantiation ceiling and a high pathological-growth safety ceiling
fail with an instance-path diagnostic. A normal code-size warning threshold
remains telemetry-driven rather than part of the language contract.

## Implemented Boundary

Sema builds concrete instances only when a direct named call reaches an
inferred scheme. Each SHA-256 content identity includes the concrete canonical
relation and constraints, normalized effect summary, reproducible checked
body, the transitive callable-dependency fingerprint, imported interface or
entry dependency facts, and compiler/backend ABI facts including target,
pointer width, compiler channel, edition, and dictionary implementation.
Changing a reachable callee body therefore invalidates its callers even when a
caller's own source representation is unchanged.

The dependency fingerprint condenses recursive callable groups into strongly
connected components before hashing their acyclic dependency graph. Collection
is linear in definitions and direct-call edges before deterministic sorting.
The session cache owns one mono item per full content digest, emits one local
definition under `__styio_mono_<name>_<sha256>`, and keeps output order stable
by digest. Repeated uses add graph edges and reuse the existing instance;
unreachable generic definitions and unreachable imported concrete helpers do
not emit code.

The current safety ceilings are 64 simultaneously expanding instances and
4,096 collected mono items per compilation. Exact same-instance recursion
reuses the active node. Type-growing recursion crosses the expansion ceiling
and reports the full concrete instance path; pathological fan-out crosses the
growth ceiling with the active path and candidate instance.

## Ownership and Cache Boundary

Single ownership is defined inside one compiler invocation: the mono-item graph
and content cache select exactly one local definition for each reachable
digest. The full digest in the symbol makes repeated compilations and
call-order changes deterministic. Reuse is compilation-session-local; no disk
or distributed cache is implied.

Imported concrete bodies are retained by callable interfaces for reproducible
downstream lowering, but only a reachable concrete entry or helper is emitted.
Imported generic bodies are specialized in the consuming compilation under the
same graph and content-identity rule.

## Runtime Boundary

Schemes and constraints remain compile-time facts. This feature must not add
boxing, witness tables, dynamic type dictionaries, or GC to generated code.

## Diagnostic Boundary

Hard ceiling failures are language-visible safety diagnostics and include the
concrete instance path. A normal code-size warning is intentionally absent
until profiling data establishes a useful threshold; implementation telemetry
must not silently become a type-system limit.

## Evolution Boundary

Profile-guided eager instances, distributed caches, explicit source
instantiation, and stable function-address identity require separate decisions.
