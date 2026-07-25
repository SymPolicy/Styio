# Explicit Binding Initialization Evidence

**Purpose:** Record repository contradictions and other-language failure lessons that justify the explicit-first-value contract.

**Last updated:** 2026-07-14

## Repository evidence

| Evidence | Current fact | Requirement impact |
|----------|--------------|--------------------|
| `docs/design/syntax/ACTIVE-SYNTAX.md` Binding Model | Canonical mutable and final bindings already contain `= expr` or `:= expr`. | Supports `REQ-BI-001`; bare `name : T` has no accepted ordinary role. |
| `src/StyioParser/NewParserExpr.cpp:135-147` | `make_default_value_for_decl_latest` maps Bool to `false`, Float to `0.0`, String to `""`, and the remaining types to integer `0`. | Violates `REQ-BI-002` and fabricates the wrong value family for Unit, aggregates, resources, and future types. |
| `src/StyioParser/NewParserExpr.cpp:2707-2744` | A typed name without an RHS invokes the helper during parsing. | This is implicit default construction, not a safe definite-assignment model. |
| `src/StyioAST/AST.hpp:2591-2633` | Binding AST paths are structured around an expression owned by the binding. | Supports the mandatory-RHS invariant in `REQ-BI-005`. |
| `src/StyioSema/TypeInfer.cpp:2016-2053` | Sema infers and records the actual RHS value family. | Removing parser synthesis simplifies Sema; no extra uninitialized state is needed. |
| `tests/newparser_internal_test.cpp:175-192` | Positive tests preserve manufactured scalar defaults. | These tests encode rejected behavior and must be replaced by negative diagnostics. |
| `src/StyioLowering/AstToStyioIR.cpp:3686-3712` | Internal resource storage has zero-fill behavior. | Resource storage needs a separate typestate audit; internal bytes are not an observable ordinary `T` (`REQ-BI-004`). |
| `src/StyioParser/NewParserExpr.cpp:2144-2185`, `src/StyioAST/AST.hpp:5053-5115`, and `src/StyioSema/TypeInfer.cpp:4210-4285` | The implementation currently treats `?| task -> name : T` as a settlement-created target, but P01.14-A rejects that classification. | Migrate result capture to the ordinary RHS `answer : T = ?| operation | fallback`; retain generic `operation -> destination` as a directional endpoint contract under `REQ-BI-003`. |

## Other-language experience

### Delayed initialization requires a complete proof system

- [Java definite assignment](https://docs.oracle.com/javase/specs/jls/se20/html/jls-16.html) needs path-sensitive rules for branches, loops, abrupt completion, constructors, and fields. Permitting a bare declaration is not a small syntax convenience once reads, cleanup, closures, and control flow must be proven safe.
- [Rust variable declarations](https://doc.rust-lang.org/reference/variables.html) can separate declaration from initialization only with compile-time use checking. Low-level uninitialized storage is deliberately separated into [`MaybeUninit<T>`](https://doc.rust-lang.org/std/mem/union.MaybeUninit.html), whose contract explains why zero bits are not a valid universal `T`.

Styio does not need that second binding lifecycle for ordinary code. Value Blocks and explicit `? | T` already express branch-produced values and deliberate absence.

### Hidden runtime state breaks the ordinary type boundary

- Kotlin [`lateinit`](https://kotlinlang.org/docs/properties.html) adds a runtime uninitialized exception, type restrictions, and initialization queries. A nominally ordinary non-null type therefore has a hidden third state.
- The [C++ object model](https://eel.is/c++draft/basic.indet) has extensive rules for indeterminate and erroneous bytes. Uninitialized reads remain an optimizer, security, and diagnostics burden.

Styio has already frozen that ordinary `T` contains values of `T`, not absence or hidden initialization states.

### Universal zero/default values are not universal usable values

- The [Go specification](https://go.dev/ref/spec#The_zero_value) recursively assigns zero values, including nil maps and channels. That is internally consistent for Go, but it also makes allocation state and domain values share one implicit policy.
- [C# nullable-reference guidance](https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/null-safety/nullable-reference-types) documents cases where `default` construction can leave reference-bearing structures with null internal state despite non-null annotations.

Styio cannot choose one valid domain value for every generic, record, resource, callable, or handle type. Explicit construction keeps policy visible and type-directed without reinterpreting missing syntax.

## Inference versus evidence

Repository code proves that the present parser manufactures values and that later binding paths expect an RHS. External specifications prove the complexity and failure modes of the main alternatives. The conclusion that Styio should delete the route follows from the owner's frozen decision; external languages inform implementation hazards but do not define Styio semantics.

## Migration evidence to collect

1. Enumerate every source fixture containing a standalone typed name with no RHS and classify it as rejected ordinary syntax, a supplied binder, a schema field, or a topology declaration.
2. Prove that all positive ordinary examples have an explicit first value.
3. Prove that settlement-result bindings use the ordinary mandatory-RHS AST/Sema route, while generic directional endpoints use the shared directional-flow route and no task-specific target declaration survives.
4. Audit internal resource zero-fill sites for read-before-valid-initialization without importing resource policy into this plan.
5. Remove old positive parser tests after the stable negative diagnostic is registered.
