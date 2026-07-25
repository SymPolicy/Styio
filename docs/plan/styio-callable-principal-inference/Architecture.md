# Callable Principal Inference Architecture

**Purpose:** Define the one semantic, specialization, IR, tooling, and cache architecture that delivers `REQ-CPI-001..011`.

**Last updated:** 2026-07-20

## Prerequisite boundaries

1. [Styio-Callable-Principal-Inference.md](../../design/Styio-Callable-Principal-Inference.md) owns the approved Q02-INF contract; this architecture implements it without reopening the decision.
2. Equality terms, principal canonicalization, and `identity` do not require numeric defaulting and may proceed.
3. Accepted decision `Q05-LIT-ADD` is linked through
   [Styio-Exact-Literals-and-Builtin-Add.md](../../design/Styio-Exact-Literals-and-Builtin-Add.md).
   Its sibling plan owns the exact-literal and scalar-`Add` catalog. The
   `add_five` integration node remains pending until that reviewed catalog
   interface is available; Q02 does not translate or store a second table.

## Architectural invariants

1. Syntax AST records authored syntax; Sema side tables record inferred semantics.
2. Inference variables exist only inside a compilation-session inference arena. `TypeTable`, SGIR, LLVM, cross-session cache keys, and diagnostics snapshots contain canonical solved/canonicalized facts only.
3. A callable definition is checked once into a principal scheme; calls instantiate the scheme and never re-typecheck an arbitrary body as a template.
4. No decision depends on future call sites, first call, allocation order, hash order, file traversal, worker schedule, or elapsed time.
5. Every emitted callable and call has one concrete signature. There is no runtime generic dictionary, erased fallback ABI, or `Undefined`/`i64` repair.
6. Only one implementation survives: old parameter mutation, return caches, default lowering, and backend call-result fallback are removed with their obsolete tests.

## Module and file ownership

| Layer / responsibility | Intended owner | Contract |
|---|---|---|
| Inference term arena | `src/StyioSema/Inference/TypeTerm.hpp/.cpp` | Own `InferenceArena`, `TypeTermId`, `TypeVarId`, structural term constructors, levels, and solved-type reification interface. No AST or diagnostics policy. |
| Constraint facts/origins | `src/StyioSema/Inference/Constraint.hpp/.cpp` | Own equality/operator variants, literal facts, compact source origin IDs, and deterministic ordering/canonical form. |
| Equality solver | `src/StyioSema/Inference/Unifier.hpp/.cpp` | Own union-find, bindings, occurs check, structural unification, bounded error trace, and solution queries. No callable eligibility or operator policy. |
| Callable schemes | `src/StyioSema/Inference/CallableScheme.hpp/.cpp` | Own quantified canonical variables, parameter/result terms, residual closed constraints, concrete completion summary reference, fingerprints, instantiation, and ambiguity/reachability validation. |
| Closed Q05 relation adapter | focused interface under `src/StyioSema/Inference/`, consuming the Q05-plan-owned numeric catalog | Translate a callable `OperatorConstraint` query into the catalog's indexed candidates and functional-determination facts. It owns no rows, defaults, materialization algorithm, arithmetic execution, open registration, or user instance API. |
| Definition/call orchestration | `src/StyioSema/CallableInference.hpp/.cpp` | Generate constraints from callable bodies, enforce eligibility/boundaries, publish `CallableSchemeTable`, `CallUseTable`, and concrete-call requests. Consume resolver, operation-summary, and operator interfaces only. |
| Sema integration/removal | `src/StyioSema/TypeInfer.cpp`, `src/StyioSema/SemaContext.hpp`, callable helpers in `src/StyioAST/AST.hpp` | Delegate callable/body/call typing to the new orchestration, expose read-only facts, and delete old mutation/cache semantics. AST changes are limited to correcting/removing misleading annotation helpers. |
| Concrete identity | `src/StyioSession/TypeTable.hpp/.cpp` and a focused `src/StyioSession/ConcreteTypeFingerprint.hpp/.cpp` if evidence requires it | Intern solved concrete types and produce canonical content encodings. The fingerprint boundary never stores inference variables or raw interner ordinals. |
| Specialization planning | `src/StyioLowering/CallableSpecialization.hpp/.cpp` | Own `DefinitionKey`, `SpecializationKey`, deterministic worklist/state machine, budgets, stable internal naming, and mapping from symbolic call-use templates to concrete call instances. |
| AST-to-concrete IR lowering | `src/StyioLowering/AstToStyioIR.cpp`, `src/StyioLowering/AstToStyioIRLowerer.hpp` | Lower each scheduled specialization under a concrete substitution; generic source definitions are templates consumed by the specializer, not directly emitted default functions. |
| Concrete IR contract | `src/StyioIR/GenIR/SGIR.hpp`, `src/StyioIR/Verifier.cpp` | Keep `SGType` concrete; make `SGFunc`/`SGCall` share a concrete instance identity/signature and reject unresolved types/signature mismatches. |
| Backend emission | `src/StyioCodeGen/GetTypeG.cpp`, `src/StyioCodeGen/CodeGenG.cpp` | Declare/define/call stable concrete names and use the concrete `SGCall` result/signature. Remove module-lookup/default result typing. |
| Diagnostics and IDE/cache | focused diagnostics files under `src/StyioSema`, `src/StyioServices/StyioIDE/CompilerBridge.cpp`, HIR/SemDB fact owners | Render canonical schemes/constraints, publish hover/call facts, and key/invalidate caches from immutable semantic fingerprints. |
| Build/test wiring | `src/cmake/StyioFrontendSources.cmake`, `src/cmake/StyioBackendSources.cmake`, `tests/CMakeLists.txt` | Register every new responsibility module in production and the relevant test targets. |

New source filenames are architecture targets and may be adjusted only by updating this document and affected checkpoint ownership before implementation. They are deliberately split so no new inference subsystem is accumulated in `TypeInfer.cpp`.

## Semantic data model

### Inference arena

`InferenceArena` owns dense IDs and vectors for locality and deterministic lifetime:

```text
TypeVarId  = dense arena-local strong ID
TypeTermId = dense arena-local strong ID

TypeTerm = Concrete(TypeId)
         | Variable(TypeVarId)
         | Apply(TypeConstructor, [TypeTermId...])

VarState = { parent, rank_or_size, optional_binding, level, origin }
```

`Apply` is used only for existing structural types whose constituents participate in inference; it does not add user-defined generic constructors. Fully concrete opaque Styio types may remain `Concrete(TypeId)`. The arena is never serialized. Each definition inference and concrete call/specialization solving context gets isolated mutable state, preventing rollback and cross-call leakage.

### Constraints and source origins

```text
Constraint = Equal(lhs, rhs, origin)
           | Operator(relation_id, operands, result, literal_facts,
                      completion_fact, origin)

ConstraintOrigin = { semantic_node_key, source_span, role, parent_origin }
```

Origins are compact IDs with bounded parent chains. They support diagnostics without retaining copied ASTs. Constraint canonicalization excludes diagnostic span from semantic identity but retains a stable representative origin for reporting.

### Schemes and facts

```text
CallableScheme = {
  definition_key,
  canonical_variables,
  parameter_terms,
  result_term,
  residual_constraints,
  concrete_completion_summary,
  eligibility_proof,
  semantic_fingerprint
}

CallUseTemplate = {
  call_site_key,
  callee_scheme_key,
  scheme_local_substitution,
  argument_terms,
  result_term
}

ConcreteCallInstance = {
  caller_specialization_key,
  call_site_key,
  callee_specialization_key,
  concrete_argument_type_ids,
  concrete_result_type_id
}
```

`CallableSchemeTable` is keyed by stable resolved definition identity. `CallUseTable` stores definition-time symbolic uses. `ConcreteCallInstanceTable` is keyed by caller specialization plus call site because one generic body call may resolve differently in different concrete caller instances. These tables replace AST setters and the single inferred-return maps.

## Constraint generation and solving

1. Create fresh terms for each unannotated parameter/result; annotated positions become concrete/structural terms.
2. Bind parameter names in a semantic term environment. A `NameAST` reads the same term; it does not read/write a concrete `ParamAST` type.
3. Literals create exact literal facts using the Q05 catalog interface. No
   early `i64` is inserted while generalization remains possible; late default
   is requested only at a Q05-defined mandatory concrete boundary.
4. Equality-producing constructs enqueue equality constraints. Calls instantiate a callee scheme into fresh terms and enqueue argument equalities plus residual constraints.
5. Binary operators enqueue one closed operator relation with
   operand/result/literal facts. The old syntax-node-kind matrix is removed in
   the coordinated Q05/Q02 migration; it is never retained as a fallback.
6. Returns/tail completion equate the body result with the callable result term and attach the upstream concrete finite operation-summary fact.
7. Run union-find equality solving with path compression/rank, structural recursion, and occurs checks. Operator candidates are selected from a finite index and may add equalities/solutions; ambiguous or empty candidate sets fail with the originating constraint.
8. Generalize eligible free variables after solving. Canonical variables are numbered by deterministic first occurrence over parameters, result, then sorted residual constraints. Normalize/deduplicate constraints and verify every quantified variable is reachable or functionally determined by the closed relation metadata.

The solver returns a `Solution` or a structured error; it never returns `Undefined` as success.

## Generalization and boundary policy

Eligibility is an explicit proof object assembled from resolver/control-flow facts: final binding, non-recursive SCC, non-boundary visibility/protocol status, and Q02/Q04-approved capture safety. Missing evidence means ineligible, not weakly polymorphic.

- Eligible definitions publish one stable scheme.
- Mutable callable creation without an expected scheme fails when parameters/results remain unresolved. A rebinding is checked against its already stable expected scheme.
- Public/exported, recursive, native/FFI, and typed-protocol boundaries require explicit contracts and do not publish hidden generic ABI.
- Explicitly typed monomorphic functions continue through the same call fact and concrete specialization interfaces, avoiding a separate code path.

## Q05 operator relation seam

The Q05 delivery plan exposes one reviewed, finite catalog whose indexed key
begins with operator and known operand/literal families. The Q02 adapter asks
that catalog for candidates and receives result equalities, representability,
completion facts, and functional-determination positions. Candidate order and
catalog fingerprints are canonical, and solver outcome is independent of
declaration order.

For `Q05-LIT-ADD`, the adapter may observe only the accepted `i8`–`i128`,
`f32`, and `f64` same-type rows, symmetric exact-literal materialization, the
same-type result, and completion facts `{overflow}` for integer rows and `{}`
for floating rows. The generalized stable bound is `{overflow}`. String,
matrix, mixed-concrete-type, future conversion, other-operator, and
instance-dependent completion rows are absent. Any new row requires its own
owner decision and catalog revision; Q02 cannot add it locally.

## Concrete type reification and fingerprints

After a call/specialization solution has no unresolved runtime-relevant term, recursively reify it to a concrete `StyioDataType` and intern it through `TypeTable`. Failure to reify is a Sema error before lowering.

Within a session, compact `TypeId` vectors support lookup. For persisted caches and mangled names, `ConcreteTypeFingerprint` encodes the canonical type structure/content, including stable type constructor/family names and parameters, then hashes with a specified deterministic repository hash. It never hashes pointer values, `std::hash`, or raw interner insertion ordinals. Fingerprint collisions are resolved by comparing the full canonical key before reuse.

## Specialization worklist and gates

Roots are concrete calls from top-level executable code and explicitly concrete callable bodies. The planner uses:

- a hash map from full `SpecializationKey` to dense record index for expected O(1) lookup;
- a queue/deque of newly discovered records;
- explicit states `Discovered`, `Lowering`, `Lowered` for cycle detection;
- a stable sorted emission list by full canonical key/internal name.

On discovery, insert the key before lowering its body. A same-key recursive edge reuses the record. A different/growing key in a recursive SCC is rejected as unsupported implicit polymorphic recursion. Every new record charges deterministic per-definition and compilation budgets before body lowering. Constraint counts/depth and generated IR estimate are charged similarly. Evidence chooses defaults and boundary tests; no timeout or silent fallback is permitted.

Generic definitions with no reachable instance produce no `SGFunc`. Each reached instance lowers the source body under a concrete substitution and resolves nested `CallUseTemplate` records into `ConcreteCallInstance` records. The same full key is emitted once regardless of call order.

## SGIR and backend contract

`SGType` remains a wrapper for solved concrete `StyioDataType`. `SGFunc` and `SGCall` share the stable internal instance name/key and concrete signature; `SGCall` owns a concrete result type so type queries do not depend on LLVM module declaration order. The verifier builds a deterministic function-signature map and checks arity, argument types, result type, uniqueness, and absence of `Undefined`.

Codegen declares all concrete functions, defines all bodies, then emits calls by stable name/signature. `GetTypeG` reads `SGCall`'s concrete result. A missing callee/signature mismatch is an IR/codegen error, never an `i64` default or argument coercion to an unrelated first instance.

## Diagnostics, IDE, and cache architecture

Stable diagnostic families distinguish:

- no principal/usable definition scheme;
- ineligible implicit generalization or missing explicit boundary contract;
- equality/occurs failure;
- ambiguous or unsatisfied closed operator relation;
- concrete call argument/result mismatch;
- implicit polymorphic recursion/specialization growth;
- specialization/constraint/code-size budget exhaustion;
- unresolved type reaching the concrete IR boundary.

CLI and IDE render the same canonical meta-notation and source-origin trace. `forall`/`Add` in hover or diagnostics describe compiler facts and are not parsed source syntax. IDE HIR/SemDB stores scheme/call-result fingerprints and source spans, not mutable inference arenas.

Definition-scheme cache keys include stable definition content/identity, relevant lexical-environment scheme fingerprints, explicit expected contract, operation-summary input, Q05 relation-catalog fingerprint, and semantic configuration. Call sets are excluded. Concrete instance keys add the canonical substitution. Dependency edges invalidate schemes, call uses, and instances transitively when any input changes.

## Dependency direction and chosen patterns

```text
AST/resolver/operation facts
        -> inference domain (terms, constraints, unifier, schemes)
        -> immutable callable/call fact interface
        -> specialization planner
        -> concrete SGIR + verifier
        -> LLVM backend

immutable callable/call facts -> diagnostics and IDE/SemDB/cache
Q05-owned catalog -> closed relation adapter -> inference domain
TypeTable <-> solved-type reification boundary only
```

- **Inference arena + union-find** earns its complexity by giving scoped mutation, near-linear equality solving, occurs safety, and compact storage.
- **Canonicalization boundary** earns its complexity by enabling deterministic cache/cycle/instance identity while keeping arena IDs local.
- **Side-table fact pattern** prevents syntax mutation and gives lowering/IDE one read-only semantic API.
- **Worklist specialization** makes reachability, deduplication, recursion, and resource accounting explicit before backend emission.
- **Closed relation adapter** isolates Q05 policy from the generic equality engine and prevents open-world coherence problems.
- No new runtime strategy, visitor hierarchy, service locator, or generic dictionary is introduced; existing AST visitor dispatch remains a thin caller of the focused inference module.

## Complexity and memory targets

- Equality unions/finds: amortized `O((V + E) α(V))`; structural traversal and occurs checks are linear in visited term edges with per-solve visitation marks.
- Operator solving: bounded by the finite indexed Q05 candidate bucket, with deterministic propagation until a monotone work queue reaches a fixed point or reports ambiguity/failure.
- Canonicalization: linear traversal plus deterministic constraint sorting/deduplication; no all-program call scan.
- Specialization discovery: expected `O(I + C)` lookups for `I` unique instances and `C` concrete call edges, plus `O(I log I)` stable emission sorting.
- Memory: dense arena vectors plus unique schemes/call uses/instances. Origins and traces are bounded; budgets are checked before growth.

## One-shot migration and deletion

1. Introduce and test the inference-domain interfaces without making them a second production authority.
2. Switch callable definition/body/name/binop/call inference together to those interfaces.
3. Delete `func_args[i]->setDataType(arg_types[i])`, `inferred_function_return_types_` and related helpers, callable `Undefined`-as-wildcard success, and misleading AST return-type helpers.
4. Switch lowering to specialization records, delete unspecified parameter/return `i64` defaults, and stop emitting generic source definitions directly.
5. Make SGCall/result/signatures concrete, switch codegen, and delete module-lookup/`i64` fallback and single-name coercion behavior.
6. Move CLI/IDE/cache consumers to immutable facts, migrate tests/docs, and delete old positive assertions and compatibility routes.

The steps describe merge ordering only; no intermediate state is an accepted deliverable and no compatibility path remains at the final checkpoint.
