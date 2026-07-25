# Styio Explicit Binding Initialization Plan

**Purpose:** Deliver the frozen rule that an ordinary Styio binding exists only after an explicit RHS supplies its first value, with no implicit default or hidden uninitialized state.

**Last updated:** 2026-07-14

**Status:** Pending implementation. Owner decisions `O01-Q08..Q09` are frozen; the current compiler still accepts and synthesizes values for a rejected bare declaration form.

## 前置条件

1. **并行：** Repository evidence, external-language evidence, fixture discovery, diagnostic inventory, and documentation consistency checks may run in parallel. Parser/AST changes and Sema/lowering invariant changes merge serially behind the architecture checkpoint because they share the binding contract.
2. **子智能体：** Sub-agents may perform read-only audits, test discovery, and disjoint documentation checks. One coordinator reconciles grammar, parser diagnostics, AST non-null RHS ownership, Sema binding state, and final SSOT wording.
3. **基座：** Reuse the current parser authority, source diagnostics, AST ownership, type table, test harness, syntax-convergence gates, and documentation workflow. Any missing general-purpose gate belongs to the `common-foundation` plan before this feature depends on it.
4. The frozen source contract is [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md), especially `O01-Q08..Q09` and the earlier `O01-Q01..Q07` value-model premises.
5. This plan does not decide record-field defaults, resource-topology initialization, restricted raw storage, FFI out-pointers, or a future explicit default-producing expression.

## Delivery target

The compiler and documentation converge on one ordinary-binding rule:

- `name [: T] = expr` creates or rebinds a mutable ordinary value;
- `name [: T] := expr` creates a final ordinary value;
- both forms always contain an explicit RHS expression;
- `name : T` is rejected for every ordinary `T`, including `? | T`, `unit`, collections, records, callable values, and handles;
- missing syntax never creates zero, `false`, an empty string, `()`, `(?)`, uninitialized storage, or an implicit `Default` value;
- parameters and pattern/iteration binders remain legal under their own supplying constructs; settlement results use an ordinary RHS such as `answer : T = ?| operation | fallback`, while typed directional endpoints belong to generic `left -> destination` rather than to a settlement binder;
- schema and resource-topology declarations retain separately owned construction/protocol rules and gain no implicit default;
- obsolete synthesized-default implementation and acceptance tests are removed in the same migration.

## Scope

1. Nightly parser rejection and one stable source-located diagnostic for a missing ordinary RHS.
2. Binding AST construction and ownership with a mandatory non-null value expression.
3. Sema, StyioIR, lowering, and codegen assertions that ordinary bindings never enter an uninitialized/default-repair state.
4. Migration of task/resource fixtures that accidentally relied on parser-synthesized values while retaining true supplied-binder syntax.
5. Positive, negative, security, parser-shadow, pipeline, editor/tooling, and active-language documentation evidence.
6. One-shot deletion of the default-value helper, its positive tests, and every executable compatibility route for bare ordinary declarations.

## Non-goals

- No general `Default` capability, constructor protocol, zero-value policy, `lateinit`, or definite-assignment language.
- No `MaybeUninit`-like source API, out-pointer API, partial aggregate construction, or raw-memory design.
- No record field-default or resource-slot initialization decision.
- No change to typed parameters, patterns, iteration binders, generic directional endpoints, schema fields, or topology declarations beyond proving their boundary from ordinary storage; settlement-target binders are not an accepted language category.
- No compatibility parser, warning-only transition, legacy AST, or retained positive fixture for the rejected spelling.

## Execution graph

The machine-validated graph is [Checkpoints.json](./styio-explicit-binding-initialization/Checkpoints.json). Plan-local delivery contracts are:

- [Requirements.md](./styio-explicit-binding-initialization/Requirements.md)
- [Evidence.md](./styio-explicit-binding-initialization/Evidence.md)
- [Validation.md](./styio-explicit-binding-initialization/Validation.md)
- [Architecture.md](./styio-explicit-binding-initialization/Architecture.md)

## 验收条件

1. Every `REQ-BI-*` requirement maps to executable or static checks in `Validation.md` and to implementation nodes in `Checkpoints.json`.
2. Bare `name : T` forms fail at the authoritative parser boundary with one stable diagnostic for representative scalar, optional, Unit, collection, record, callable, and handle types.
3. Explicit `(?)` and `()` initializers work; typed parameters, patterns, and iteration binders remain accepted through their own routes; `answer : T = ?| operation | fallback` is verified as an ordinary binding with a real RHS, and `?| (operation -> destination) | fallback` remains generic operation composition rather than a settlement-created declaration.
4. `make_default_value_for_decl_latest`, its default-synthesis route, and positive compatibility tests are absent; AST/Sema/IR ordinary-binding paths require a real RHS.
5. Resource storage bytes cannot become an observable ordinary `T` before valid protocol initialization; any deeper topology issue is recorded under its owning plan instead of repaired with a source default.
6. Parser, language-feature, pipeline, security, parser-shadow, syntax-convergence, documentation, lifecycle, local-information, and Better Plan validators pass on one head commit.
