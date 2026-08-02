# Affine Capturing Closures

**Purpose:** Own the capture-ownership and escape rules for callable values whose explicit `$(...)` environment is derived as borrow or consume rather than copied or garbage-collected implicitly.

**Last updated:** 2026-07-31

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.affine-capturing-closures"
title = "Affine Capturing Closures"
kind = "closure-ownership"
decision_state = "accepted"
delivery_state = "converged"
owner = "Sema / Resource Topology"
syntax = "Place the explicit `$(name, ...)` capture list between a callable signature and its binding/body operator, for example `# add : i64 $(seed) := (value: i64) => value + seed`; no lifetime, copy, heap, or ownership keyword is introduced."
resolution = "Derive shared-borrow, exclusive-borrow, or consume facts from each captured use; lower the proven scalar slice to one program-static environment; permit shared-borrow escape, keep exclusive-borrow environments direct-call-only, and reject consume or representation/drop gaps before lowering."
golden_cases = [
  "tests/features/affine_closures/t01_shared_static_escape.styio",
  "tests/features/affine_closures/t02_exclusive_static_direct.styio",
  "tests/features/affine_closures/t03_capture_name_scope_isolation.styio",
  "tests/features/affine_closures/t04_static_scalar_families.styio",
  "tests/features/affine_closures/e01_missing_capture.styio",
  "tests/features/affine_closures/e02_exclusive_escape.styio",
  "tests/features/affine_closures/e03_missing_drop_path.styio",
  "tests/features/affine_closures/e04_duplicate_capture.styio",
  "tests/features/affine_closures/e05_consume_without_drop_proof.styio",
  "tests/features/affine_closures/e06_unused_capture.styio",
]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md", "docs/design/Styio-Resource-Topology.md", "docs/design/Styio-Handle-Capability-Type-System.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/CALLABLE-TYPE-EVOLUTION-QUESTIONS-2026-07-31.md"]
implementation = ["src/StyioAST/AST.hpp", "src/StyioParser/HashFunctionParser.hpp", "src/StyioParser/NewParserExpr.cpp", "src/StyioSema/SemaContext.hpp", "src/StyioSema/TypeInfer.cpp", "src/StyioIR/GenIR/SGIR.hpp", "src/StyioLowering/AstToStyioIR.cpp", "src/StyioCodeGen/GetTypeG.cpp", "src/StyioCodeGen/CodeGenG.cpp"]
evidence = ["tests/features/affine_closures/t01_shared_static_escape.styio", "tests/features/affine_closures/t02_exclusive_static_direct.styio", "tests/features/affine_closures/t03_capture_name_scope_isolation.styio", "tests/features/affine_closures/t04_static_scalar_families.styio", "tests/features/affine_closures/e01_missing_capture.styio", "tests/features/affine_closures/e02_exclusive_escape.styio", "tests/features/affine_closures/e03_missing_drop_path.styio", "tests/features/affine_closures/e04_duplicate_capture.styio", "tests/features/affine_closures/e05_consume_without_drop_proof.styio", "tests/features/affine_closures/e06_unused_capture.styio"]

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
ownership-contract = "docs/design/Styio-Handle-Capability-Type-System.md"
topology-contract = "docs/design/Styio-Resource-Topology.md"

[implementation]
path = "src/StyioSema/TypeInfer.cpp"
symbol = "validate_affine_capture_environment"
owner = "Sema / Resource Topology"

[dependencies]
requires = [
  { id = "core.monomorphic-callable-values", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.canonical-effect-rows", decision_state = "accepted", delivery_state = "converged" },
  { id = "core.capability-usage-polymorphism", decision_state = "accepted", delivery_state = "converged" },
]
requires_any = []
extends = ["core.monomorphic-callable-values"]
conflicts = []
supersedes = []
after = ["core.canonical-effect-rows", "core.capability-usage-polymorphism"]
```

## Decision

Q2-A was approved as a blocked direction on 2026-07-31. Styio reuses the
visible `$(...)` capture list and derives how each capture is used. A read-only
capture is a shared borrow, a mutation requires an exclusive borrow, and a
value moved into an escaping environment is consumed.

The environment is affine: it may be invoked according to the checked callable
use contract, but it is never assumed freely copyable. Nonescaping
environments may remain stack/static. Escape is legal only when every captured
field has a deterministic owned representation, transfer rule, and drop path.

## Delivered Boundary

The authoritative callable grammar accepts a nonempty, duplicate-free capture
list after the parameter/result signature and before `=`, `:=`, or `=>`.
Sema requires that this list exactly names the callable's free value
environment. Missing and unused names fail deterministically; implicit capture
discovery is not used to widen the executable closure surface.

The delivered environment is program-static and reactive by reference.
`bool`, integer, floating-point, and `char` bindings have a concrete static
slot visible to the callable body. Reads derive `shared_borrow`; rebinding a
captured mutable scalar derives `exclusive_borrow`; destroy/acquire transfer
forms derive `consume`. Shared-borrow environments may freeze to one complete
monomorphic callable type. Exclusive-borrow environments remain direct-call
only, while consume environments fail until a unique invocation and drop path
can be proven.

Resource, stream, task, and topology handles must retain their existing state
and ordering identities inside an environment. Capturing does not normalize
them to a shared integer representation. Strings and materialized container
handles also remain closed because the current static environment has no
approved ownership transfer and drop record for them.

## Diagnostic and Compatibility Boundary

Diagnostics identify the capture, derived use mode, escape point, and missing
transfer/drop fact. They must reject aliasing an exclusive borrow, using a
consumed capture, or allowing a borrowed value to outlive its checked scope
before lowering.

Existing reactive capture syntax remains valid for its current monomorphic
semantics. This feature does not silently make an existing capture escaping,
copyable, reference-counted, or heap allocated.

An imported captured callable is rejected because portable StyioIR body
schema v1 does not carry an environment initializer or module-owned
static-storage record. Cross-module captured environments must extend the
portable-body/interface feature rather than borrowing a consumer module's
binding by name.

## Evolution Boundary

Authored lifetime names, reference counting, cyclic closure environments,
implicit capture discovery, generalized capturing closures, and a garbage
collector are not approved by this feature.

## Verification

Run:

```bash
ctest --test-dir build -L affine_closures --output-on-failure --no-tests=error
```

The positive fixtures cover shared static escape through an invariant callable
value, exclusive static mutation through repeated direct calls, isolation from
same-spelled local bindings in noncapturing functions, and native
`bool`/`f64`/`char` storage widths. The negative fixtures pin exact capture
sets, duplicate syntax, exclusive escape, unsupported drop/representation
facts, and consume-mode rejection.
