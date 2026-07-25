# Styio Callable Principal Inference Plan

**Purpose:** Deliver Q02-INF as deterministic definition-site principal inference with independent call instantiation and concrete monomorphization, while removing first-use AST mutation and backend type defaults.

**Last updated:** 2026-07-20

**Status:** Pending implementation. Q02-INF and `Q05-LIT-ADD` are approved;
operator-constrained inference consumes the separately delivered Q05 exact-literal
and scalar-`Add` catalog rather than owning a second numeric implementation.

**Better Plan ID:** `5a5ec867-9998-4e78-9111-761cad834754`

## 前置条件

1. **语义所有者：** The approved Q02-INF contract is owned solely by [Styio-Callable-Principal-Inference.md](../design/Styio-Callable-Principal-Inference.md). This plan may refine implementation evidence but may not alter that language policy.
2. **Q05 integration:** The accepted finite contract is owned by
   [Styio-Exact-Literals-and-Builtin-Add.md](../design/Styio-Exact-Literals-and-Builtin-Add.md)
   and its delivery lifecycle is
   [Styio-Exact-Literals-and-Builtin-Add-Plan.md](./Styio-Exact-Literals-and-Builtin-Add-Plan.md).
   This plan owns retention and solving of the callable's closed constraint; it
   does not duplicate exact-literal storage, materialization, scalar rows,
   checked arithmetic, or constant evaluation. No implementation may substitute
   the current ad hoc `+` matrix or an `i64` default for that catalog.
3. **Existing callable boundary:** Reuse the accepted Q02-SIG explicit-boundary and finite-completion contract. Public/exported, recursive, native/FFI, and typed-protocol boundaries remain explicit; this plan does not create source-visible generic or constraint syntax.
4. **并行：** Read-only evidence, test discovery, IDE/cache inventory, and monomorphization research may run in parallel. Implementation branches may run in parallel only after the shared inference interfaces are frozen and only when their file ownership is disjoint.
5. **子智能体：** Sub-agents may perform those read-only or disjoint lanes. One coordinator owns the serial Sema inference core, callable scheme publication, call-instantiation migration, and final convergence so no duplicate inference authority survives.
6. **基座：** Reuse `CompilationSession`, `TypeTable`, symbol resolution, StyioIR verification, diagnostics, IDE semantic facts, and the current test harness. General-purpose workflow or test substrate belongs to `common-foundation`, not to a feature-local replacement.

## Delivery target

The compiler has one callable-inference pipeline:

- eligible final, non-recursive, non-boundary, capture-safe callable definitions are checked once at their definition and publish a canonical rank-1 principal constraint scheme;
- `# identity := (x) => x` publishes one quantified parameter/result identity relation without reading future calls;
- `# add_five := (x) => x + 5` preserves a closed built-in operator/literal constraint supplied by Q05 instead of choosing a concrete numeric type;
- each call fresh-instantiates the scheme, solves its own constraints, and records a source-located call-use fact without modifying the shared AST;
- solved concrete types are interned through `TypeTable`, and a deterministic specialization worklist emits or reuses concrete `SGFunc`/`SGCall` instances;
- SGIR and LLVM never receive an unresolved type variable, `Undefined` runtime type, or implicit `i64` repair;
- recursion growth, instance count, and code-size expansion fail through deterministic, configurable compiler gates rather than time-based or order-dependent behavior;
- CLI diagnostics, IDE hover/navigation facts, and incremental caches consume the same canonical scheme and call-instantiation records.

## Scope

1. Sema-only `TypeTerm`, `TypeVarId`, equality/operator constraints, source origins, union-find unification, occurs checking, scheme generalization, canonicalization, ambiguity checking, and instantiation.
2. Definition and call side tables keyed by resolved semantic identity; no inferred semantic state is written into syntax AST nodes.
3. Integration of the Q05-owned closed operator/literal catalog into callable
   constraint retention and solving, including its stable `{overflow}` upper
   bound; the numeric catalog itself remains Q05-plan-owned.
4. Concrete `TypeId` reification, stable specialization keys/names, deterministic worklist monomorphization, recursion/expansion gates, concrete-only SGIR, and backend emission.
5. Stable diagnostics, IDE facts, cache keys/invalidation, focused performance evidence, migrated tests, obsolete-test deletion, and documentation convergence.
6. One-shot deletion of first-call `ParamAST` mutation, per-function single inferred-return caches, unresolved callable `i64` defaults, and every executable fallback that preserves those semantics.

## Non-goals

- No source `forall`, trait, type-parameter, constraint, overload, default-argument, variadic, or runtime type-dictionary syntax.
- No user-defined operator instances, public hidden generic ABI, higher-rank/impredicative polymorphism, implicit polymorphic recursion, or open completion rows; those remain under F02/Q10 or later owner decisions.
- No capture/borrow/escape policy beyond the Q02-approved capture-safe eligibility gate; Q04 owns the general rule.
- No redefinition or extension of accepted `Q05-LIT-ADD`, and no decision about
  its still-deferred conversion syntax, other operators, text/container/matrix
  relations, additional numeric types, or NaN comparison policy.
- No whole-program call-site inference, first-use fixing, C++-template-style arbitrary body rechecking, runtime erasure ABI, compatibility inferencer, or retained backend default.

## Execution graph

The machine-validated graph and delivery contracts are under [styio-callable-principal-inference](./styio-callable-principal-inference/Plan.md):

- [Requirements.md](./styio-callable-principal-inference/Requirements.md)
- [Evidence.md](./styio-callable-principal-inference/Evidence.md)
- [Validation.md](./styio-callable-principal-inference/Validation.md)
- [Architecture.md](./styio-callable-principal-inference/Architecture.md)
- [Checkpoints.json](./styio-callable-principal-inference/Checkpoints.json)

## 验收条件

1. Every `REQ-CPI-*` requirement maps to implementation nodes and executable/static evidence, and final validation covers every label on one head commit.
2. Same-module calls `identity(n)` and `identity(s)`, where `n: i64` and `s: string` are explicitly typed bindings, both succeed in either source order, share one principal scheme, produce distinct concrete instances where ABI types differ, and never mutate the definition AST.
3. `add_five` remains symbolic in its definition scheme; every accepted and
   rejected instance follows only the accepted Q05 relation catalog and reports
   definition ambiguity separately from call constraint failure.
4. `TypeTable` contains only solved canonical concrete types; scheme and instantiation caches use canonical stable keys rather than raw pointers, unordered iteration, first-use order, or raw session-local IDs.
5. SGIR verification proves every emitted function/call signature concrete and matched; codegen contains no unresolved-call `i64` fallback.
6. Direct same-instance recursion terminates where explicitly supported, implicit polymorphic recursion fails closed, and deterministic specialization limits have boundary and stress tests.
7. Old mutation, single-return-cache, default-lowering, compatibility code, and tests that require them are removed in the same migration.
8. Targeted Sema/lowering/IR/codegen/IDE tests, runtime multi-instantiation cases, determinism/cache/performance checks, structural removal searches, documentation gates, and Better Plan validation pass.
