# Callable Principal Inference Requirements

**Purpose:** Define the product contract for Q02-INF definition-site principal schemes, independent call instantiation, and concrete specialization.

**Last updated:** 2026-07-20

## Decision trace

1. The approved Q02-INF policy is owned solely by [Styio-Callable-Principal-Inference.md](../../design/Styio-Callable-Principal-Inference.md). This plan traces implementation to that authority and cannot redefine it.
2. The accepted Q02-SIG callable-boundary and finite-completion contract remains upstream. This plan changes internal inference and code generation, not source signature spelling.
3. Accepted decision `Q05-LIT-ADD` is owned solely by
   [Styio-Exact-Literals-and-Builtin-Add.md](../../design/Styio-Exact-Literals-and-Builtin-Add.md).
   Its sibling plan owns the concrete catalog and execution semantics;
   `REQ-CPI-005` owns only callable-constraint retention and consumption of that
   catalog.
4. F02/Q10 retain author-written generics/constraints, public generic surface, user instances, and generic ABI; Q04 retains the general capture/borrow/escape rule.

## Users and outcomes

- **Local Styio authors** can express type-preserving adapters and closed built-in operator-constrained helpers without redundant concrete annotations.
- **Library and boundary authors** retain explicit, reviewable contracts; hidden local schemes never leak into public/FFI/protocol ABI.
- **Compiler maintainers** get one inference authority with deterministic algorithms, bounded specialization, and concrete backend inputs.
- **IDE/tooling authors** consume the same canonical scheme, constraint origins, call results, and invalidation keys as CLI compilation.
- **Review and operations maintainers** can prove that compile results do not depend on call order, hash iteration, worker scheduling, or backend fallback.

## Functional requirements

### REQ-CPI-001 — Definition-site principal callable schemes

Every eligible unannotated callable is checked from its definition, lexical environment, and explicit expected contract only. `# identity := (x) => x` derives a canonical rank-1 scheme whose parameter and result are the same quantified variable. Future/downstream calls, first-use order, incremental workspace scope, and codegen demand do not participate in scheme derivation.

### REQ-CPI-002 — Scoped type terms, constraints, and sound unification

Sema owns typed inference arenas containing stable `TypeVarId`, `TypeTerm`, equality and closed operator constraints, source-located origins, substitutions, and solutions. Equality unification uses an occurs check plus union-find with path compression and rank/size balancing. Recursive type constructors are represented structurally where existing Styio types can contain inferred terms. Mutable inference variables are never global `TypeTable` entries, AST annotations, SGIR types, or cache keys.

### REQ-CPI-003 — Safe, stable rank-1 generalization

Only Q02-approved final (`:=`), non-recursive, non-boundary, capture-safe callable values may be implicitly generalized, and only variables not free in the lexical environment are quantified. Mutable `# f =` without an existing expected scheme does not generalize or retain a weak first-use variable; rebindings must satisfy the existing stable scheme. Public/exported, recursive, native/FFI, and typed-protocol boundaries remain explicit. Unusable or ambiguous schemes fail at the definition with source-located evidence.

### REQ-CPI-004 — Independent call instantiation and call-use facts

Each call fresh-instantiates the callee scheme, equates argument terms, solves applicable closed constraints, and determines its result independently. Symbolic call-use templates inside generic definitions and concrete call instances inside concrete caller specializations are recorded in side tables keyed by resolved call-site/definition identity, never by AST mutation. Given explicitly typed `n: i64` and `s: string`, `identity(n)` cannot constrain or invalidate `identity(s)`; identical concrete uses reuse one canonical instance.

### REQ-CPI-005 — Q05-owned exact literal and operator relations

`# add_five := (x) => x + 5` retains an operator relation over the
parameter, exact literal, result, and stable completion upper bound. It consumes
the accepted Q05 catalog: the legal concrete domain is `i8`–`i128`, `f32`, and
`f64`; concrete operands are same-type; a literal materializes symmetrically to
the other operand; result type is that concrete type; and the generalized bound
is the conservative union `{overflow}`. The solver indexes the externally owned
catalog deterministically, reports unsatisfied concrete calls at the call site,
and never falls back to the current syntax-kind matrix, a guessed numeric type,
or a second Q02-local copy of the rows.

### REQ-CPI-006 — Solved concrete type identity and stable keys

Only fully solved types are reified and interned through the session `TypeTable`. A specialization key combines stable resolved definition identity with a canonical ordered encoding of the concrete substitution/signature and relevant closed semantic-catalog fingerprint; raw pointers, unordered iteration, process hashes, and raw session-local `SymbolId`/`TypeId` ordinals are not persisted or embedded as cross-session identities. Canonical scheme and instance fingerprints are reproducible for identical source and semantic inputs.

### REQ-CPI-007 — Deterministic concrete monomorphization and SGIR

A deduplicating worklist discovers reachable concrete callable instances, assigns a stable internal name, lowers each instance under its concrete substitution, and emits one concrete `SGFunc` per key. `SGCall` carries or references the same concrete instance signature, including a concrete result type. Generic definitions without reachable concrete uses emit no runtime function. SGIR verification rejects unresolved/`Undefined` function arguments, results, binary operands/results, and calls; LLVM codegen receives no type variables or generic dictionaries.

### REQ-CPI-008 — Recursion, expansion, and resource gates

Implicit polymorphic recursion is rejected. Same-key cycles are represented once where an explicit recursive contract permits them; a recursive edge that grows or changes the specialization key fails deterministically. Per-definition, per-module/session, constraint-count, and total generated-code budgets are explicit compiler settings with evidence-backed defaults, checked before unbounded allocation/emission, and produce stable diagnostics. Limits depend on counts/structure, never elapsed time or worker order.

### REQ-CPI-009 — Shared diagnostics, IDE facts, and incremental caches

Constraint origins preserve definition/call source locations and explain expected/found terms, unsatisfied relation, ambiguity, occurs failure, ineligible generalization, recursive growth, or specialization-budget failure. CLI and IDE publish the same canonical scheme and per-call result facts using compiler meta-notation rather than new source syntax. Cache keys include the definition, relevant lexical-environment scheme fingerprints, explicit expected contract, Q05 relation-catalog fingerprint, and semantic configuration; call-site sets are excluded from definition-scheme keys, and invalidation removes every dependent call/instance fact.

### REQ-CPI-010 — Existing boundary and operation-summary preservation

Q02-SIG explicit boundaries, exact arity, and finite completion upper bounds remain enforced. A local inferred callable scheme carries the one concrete finite completion summary determined by the accepted operation analysis; this plan introduces no completion-row variable. Q05 operator relations may contribute only the finite completion facts authorized by Q05. Public/recursive/native/protocol calls cannot consume a hidden inferred generic ABI.

### REQ-CPI-011 — One inference authority and complete migration

The migration deletes first-call `ParamAST::setDataType` inference, per-function single concrete inferred-return caches, unspecified callable parameter/return `i64` lowering, module-lookup call-result fallback, and tests or docs that require those behaviors. Callable annotation predicates are corrected or removed rather than bypassed. No feature flag, compatibility scheme, old visitor branch, shadow cache, or backend repair remains; parser syntax and syntactic AST nodes remain unchanged except for removing misleading semantic helpers.

## Non-functional constraints

1. Equality solving should be near-linear in generated terms/constraints: `O((V + E) α(V))` amortized for union-find operations plus structural traversal/occurs checks. Closed operator lookup must use bounded indexed candidates, not scan all call sites or all program definitions.
2. Scheme canonicalization and specialization emission are deterministic under different allocation, hash, call, file-processing, and worker orders.
3. Inference arenas and call/instance tables are compilation-session owned and released with the session. No raw AST pointer or local runtime data crosses an incremental-cache boundary.
4. Diagnostics retain bounded traces and source origins without copying whole AST subtrees. Cycle and budget checks occur before recursive allocation can exhaust memory.
5. New modules have one responsibility, explicit interfaces, and build registration; `TypeInfer.cpp`, `AstToStyioIR.cpp`, or `CodeGenG.cpp` must not absorb the entire new subsystem.
6. Security and privacy gates remain unchanged: diagnostics/caches never serialize host paths beyond existing source-location policy, process data, secrets, or runtime values.

## Scope

- Eligible named local/module-private hash callables using existing syntax and source-reachable expression/type constructors.
- Equality, closed Q05 operator/literal constraints, concrete TypeTable reification, call instantiation, specialization, IR/backend, diagnostics, IDE, cache, and validation integration.
- Existing explicitly typed callables as regression controls and concrete recursion anchors.
- Complete removal of the displaced implementation and obsolete positive tests.

## Non-goals

- Source-visible generics/constraints, user-defined relations/instances, overload sets, default arguments, variadics, higher-rank or impredicative types.
- Implicit polymorphic recursion, open-world trait resolution, runtime dictionaries, type erasure, reflection, or a dynamic top type.
- General capture/ownership inference, public generic ABI, module export design, or numeric/operator policy owned by Q04/F02/Q10/Q05.
- Parser grammar changes for the two target definitions.

## Final acceptance target

On one head commit, the accepted Q02 owner maps to one implementation: eligible definitions produce unique usable canonical schemes, calls instantiate independently, Q05 relations alone govern operator-constrained schemes, solved TypeIds drive deterministic bounded monomorphization, SGIR/backend are fully concrete, IDE/cache/diagnostics share the same facts, explicit boundaries remain intact, and every first-use/single-return/default compatibility path and obsolete test is absent. Every `REQ-CPI-*` label has recorded positive, negative, structural, determinism, resource-bound, and end-to-end evidence.
