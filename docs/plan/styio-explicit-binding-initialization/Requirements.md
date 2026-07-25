# Explicit Binding Initialization Requirements

**Purpose:** Define the source-observable requirements for explicit first values and the rejection of implicit ordinary-binding defaults.

**Last updated:** 2026-07-14

## Decision trace

Owner decisions `O01-Q08..Q09` in `docs/review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md` freeze Option A: “没有 RHS 的定义本来就是不接受的, 不允许隐式给一个默认值.” Earlier decisions establish that ordinary `T` excludes absence, `? | T` represents possible absence, `unit` has the sole value `()`, and Block result rules do not invent values.

## Users and outcomes

1. Authors can tell from source exactly where every ordinary binding receives its first value.
2. Library and compiler authors never have to guess whether a typed name contains zero, absence, Unit, a valid domain value, or uninitialized storage.
3. Tooling can parse a complete binding without type-directed insertion of an expression.
4. Low-level and resource subsystems keep storage/protocol states behind explicitly typed boundaries instead of leaking them into ordinary `T`.

## Functional requirements

### REQ-BI-001 — Explicit first value

Every standalone ordinary storage binding uses `name [: T] = expression` or `name [: T] := expression`. The binding becomes visible only after the RHS successfully produces a value compatible with the declared or inferred type. A standalone `name : T` is rejected for every ordinary `T`.

### REQ-BI-002 — No implicit default or hidden value state

A missing RHS never creates zero, `false`, an empty string, `()`, `(?)`, an uninitialized slot, or a type-provided default. The initial language exposes no `Default` capability. Any future default-producing feature is an explicit expression governed by a separate decision.

### REQ-BI-003 — Supplied binders and directional endpoints remain distinct

Parameters, pattern binders, and iteration binders remain legal without an ordinary RHS because the call, match, or iteration supplies their values under its own grammar. Settlement returns an expression value and therefore enters storage through an ordinary RHS such as `answer : T = ?| operation | fallback`. A typed destination/location/receiver in generic `left -> destination` is an endpoint contract, not an empty declaration or a settlement-created target. These AST and Sema paths must remain distinct from empty ordinary storage.

### REQ-BI-004 — Schema, protocol, and raw-storage boundary

Record field schemas and resource-topology slot declarations describe shapes or protocols and are not ordinary executable storage bindings. This distinction grants neither an implicit field default nor an observable pre-initialized resource value. Out-pointers, partial aggregate construction, and uninitialized raw storage require a separately typed restricted API if introduced later.

### REQ-BI-005 — One mandatory-RHS compiler invariant

The authoritative parser, binding AST, Sema, StyioIR, lowering, and codegen share one invariant: every ordinary binding owns a real RHS expression and a proven value type. The parser performs no type-directed default synthesis; later layers perform no missing-value repair. Obsolete helpers, routes, and positive tests are removed rather than retained for compatibility.

### REQ-BI-006 — Stable diagnostics and converged tooling

Missing ordinary RHS syntax produces one stable, source-located parser diagnostic. Editor grammars, formatters, active language docs, fixtures, runbooks, and syntax evidence distinguish ordinary bindings from supplied binders and schema/protocol declarations. Negative migration coverage may preserve the rejected spelling only as a diagnostic fixture.

## Non-functional constraints

1. Parsing remains syntax-directed and does not query types to manufacture an RHS.
2. The change must reduce, not add, binding lifecycle states in Sema and control-flow analysis.
3. No ordinary-binding read may require a runtime initialized-bit check.
4. Diagnostics identify the binding head and state that `=` or `:=` plus an expression is required.
5. The migration is one-shot: no legacy parser, warning mode, compatibility AST, or fallback default path remains.

## Scope

`src/StyioParser`, binding nodes in `src/StyioAST`, binding/type state in `src/StyioSema`, affected `src/StyioIR` and `src/StyioLowering` assertions, editor/formatter mirrors, parser/unit/security/pipeline tests, task/resource fixtures that relied on the old behavior, and the active language documentation set.

## Non-goals

- Record construction defaults and resource-topology initialization policy.
- A public raw-storage, partial-initialization, or FFI out-pointer API.
- The syntax or semantics of a future explicit default-producing expression.
- General callable signature, generic, effect, or ABI decisions.

## Final acceptance target

On one head commit, all mapped checks pass; every `REQ-BI-*` behavior has positive, negative, or static evidence; no ordinary `BindingAST` can exist without an RHS; no parser helper synthesizes a value; supplied-binder and schema/protocol contexts remain accepted through their own routes; and all active SSOT documents state the same explicit-first-value rule.
