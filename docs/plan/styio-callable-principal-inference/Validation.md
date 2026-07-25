# Callable Principal Inference Validation

**Purpose:** Map every Q02-INF requirement to executable, structural, determinism, resource-bound, tooling, and documentation evidence before implementation begins.

**Last updated:** 2026-07-20

## Validation principles

1. Definition facts are tested independently from call facts; no positive test may pass only because a first call mutates a definition.
2. Multi-instantiation cases run in both call orders and compare canonical schemes, diagnostics, StyioIR, instance sets/names, and runtime output.
3. Equality-only inference is not evidence for `Add`; operator cases are enabled only from the cited Q05 table and include every accepted/rejected relation row.
4. Parser acceptance is a regression control, not an inference implementation test. Sema, lowering/IR, backend, IDE/cache, and removal checks are all required.
5. New internal tests must be registered in a built CMake/CTest target; an unreferenced test source is not evidence.

## Requirement matrix

| Requirement | Positive/executable evidence | Negative/static/determinism evidence |
|---|---|---|
| `REQ-CPI-001` | Definition-only and called `identity` derive the same canonical `forall A. (A) -> A`-equivalent fact; annotated controls remain concrete. | No call-set scan; swapping/removing calls does not change the scheme; unconstrained/unusable definitions report at the definition. |
| `REQ-CPI-002` | Unifier unit tests cover variable-variable, variable-concrete, structural terms, repeated variables, occurs check, and solved reification. | Arena IDs never enter AST/TypeTable/SGIR/cache; stress evidence meets near-linear target and bounded trace behavior. |
| `REQ-CPI-003` | Eligible final non-recursive non-boundary capture-safe callables generalize; existing expected-scheme rebind checks succeed. | Mutable unseeded, recursive implicit, boundary, and capture-ineligible cases demand explicit contracts; no weak first-use variable remains. |
| `REQ-CPI-004` | With explicit `n: i64` and `s: string` bindings, `identity(n)` and `identity(s)` both work in one module/order permutations; same-type repeated calls reuse one instance; nested generic calls resolve per caller instance. | One call cannot constrain another; call tables key symbolic templates and concrete caller instances correctly; AST remains byte-for-byte semantically unmutated. |
| `REQ-CPI-005` | Catalog-driven tests cover every accepted signed-integer and floating `Add`/literal row, same-type result, symmetric materialization, and generalized `{overflow}` bound. | Mixed concrete types, unsupported families, unrepresentable literals, and unhandled `overflow` match the Q05/Q01 diagnostic boundary; no current syntax-kind matrix, Q02-local row copy, or eager literal default decides a relation. |
| `REQ-CPI-006` | Solved types intern to equal TypeIds in-session; canonical scheme/type/instance fingerprints are stable across fresh sessions and allocation/order permutations. | Collision test compares full keys; persisted/mangled identity contains no pointer, `std::hash`, or raw interner/TypeId ordinal dependency. |
| `REQ-CPI-007` | Reachable instances produce one concrete SGFunc each; SGCalls have matching concrete signatures/results and execute correct outputs; unused generics emit no function. | Verifier rejects `Undefined`, unresolved variables, duplicate mismatched names, arity/type/result mismatch, and missing callee; codegen has no result fallback. |
| `REQ-CPI-008` | Same-key explicit recursion terminates where supported; configured boundary-count compilations succeed; metrics record deterministic counts. | Implicit/growing polymorphic recursion and one-over-budget cases fail before unbounded allocation with stable diagnostics in different orders/thread counts. |
| `REQ-CPI-009` | CLI and IDE hover/call facts agree on canonical schemes/results; warm-cache and cold-cache results are equal; relevant edits invalidate dependents only. | Definition cache key excludes future call set; source-located constraint traces are bounded and contain no host/runtime/private data beyond policy. |
| `REQ-CPI-010` | Explicit public/recursive/native/protocol signatures and finite completion summaries remain accepted and calls preserve their summaries. | Hidden inferred generic ABI, completion-row variables, wrong arity, and operator completions outside Q05/Q02-SIG fail closed. |
| `REQ-CPI-011` | All callable routes use the new fact interface and existing syntax still parses. | Static searches and targeted tests prove first-use mutation, single-return cache, callable `i64` defaults, module-lookup fallback, compatibility branches, and obsolete positive tests are absent. |

## Required source cases

### Accepted Q02 equality core

```styio
# identity := (x) => x
n: i64 := 7
s: string := "seven"
int_value: i64 := identity(n)
text_value: string := identity(s)
again: i64 := identity(n)
```

The same program with the string and integer calls reversed must publish the same scheme and canonical instance set. Additional controls cover explicit scalar/string/list/optional/matrix types already supported by the concrete type system, without adding generic source syntax.

### Accepted Q05 relation integration

```styio
# add_five := (x) => x + 5
```

Concrete calls are generated from the catalog owned by
[Styio Exact Literals and Built-in Add](../../design/Styio-Exact-Literals-and-Builtin-Add.md):
`i8`–`i128`, `f32`, and `f64` same-type rows; symmetric exact-literal
materialization; same-type results; checked integer `overflow`; strict floating
rows; and the stable generalized `{overflow}` upper bound. Fixtures cover both
literal positions, representability edges, every integer width, both floats,
mixed-concrete and unsupported-family rejection, and call-order independence.
They do not add text/matrix rows, conversions, or a new source spelling.

### Definition-site rejections

- A callable whose quantified variable is neither present in parameter/result positions nor functionally determined by an accepted closed relation.
- Mutable `# f = ...` with unresolved terms and no expected stable scheme.
- Implicitly generalized recursive callable, public/exported/native/FFI/protocol boundary, or capture-ineligible callable.
- Infinite structural type/occurs-check case that is source-reachable.
- A definition with no unique usable principal scheme.

### Call-site rejections

- Concrete argument equality mismatch against an explicit or instantiated scheme.
- The Q05 catalog has no candidate, a concrete type mismatch or unsupported
  family occurs, or the exact literal cannot materialize. Integer overflow is
  instead the accepted `overflow` completion edge; only failure to settle or
  propagate it is a static completion-contract error.
- Growing/different-key recursive specialization.
- Per-definition, module/session, constraint-depth/count, or code-size budget exceeded by exactly one unit.

## Unit and integration test ownership

1. `tests/typeinfer_internal_test.cpp` or a newly registered focused inference target covers terms, unifier, schemes, eligibility, ambiguity, source origins, and call facts. Its execution registration is verified in `tests/CMakeLists.txt`.
2. `tests/lowering_internal_test.cpp` covers worklist reachability, canonical keys, deduplication, unused schemes, recursion states, budgets, and concrete SGFunc/SGCall signatures.
3. `tests/codegen_internal_test.cpp` plus end-to-end `tests/styio_test.cpp` cover declaration order, concrete calls/results, runtime outputs, missing-signature rejection, and absence of backend defaulting.
4. `tests/ide/styio_ide_test.cpp` covers canonical hover text, call result facts, cross-edit stable identities, cache invalidation, and CLI/IDE agreement.
5. `tests/security/styio_security_test.cpp` covers bounded pathological constraints/specializations, deterministic failures, diagnostic data boundaries, and no resource exhaustion before gates.
6. Parser/nightly shadow tests prove the two source forms still parse without a new token/grammar route.

## Determinism and cache matrix

For representative identity, nested-generic-call, repeated-instance, Q05 operator, explicit-recursion, and budget-error modules, compile at least these permutations:

- reversed independent call order;
- unrelated function inserted before/after the definition;
- cold and warm semantic caches;
- clean session with different symbol/type interning order induced by unrelated declarations;
- supported worker/thread counts or scheduling seeds used by the build;
- repeated builds of identical source.

Compare canonical scheme fingerprints, diagnostics (excluding allowed environment-neutral timestamps), specialization full keys/internal names, sorted SGIR function/call signatures, instance counts, runtime output, and cache hit/invalidation facts.

## Complexity, growth, and stress evidence

1. Generate long equality chains, repeated diamonds, structural terms, and occurs failures; record term/constraint count, union/find operations or equivalent metrics, peak arena memory, and wall-clock trend without using time as a semantic limit.
2. Compile repeated identical calls and prove unique-instance count remains one per full key.
3. Compile distinct concrete identity uses up to the accepted per-definition/module limits and one beyond each boundary; verify pre-allocation deterministic failure.
4. Exercise same-key recursion and growing-key rejection. No test relies on exhausting process memory or stack.
5. Record generated SGIR/LLVM function counts and object/JIT code-size estimates against the configured budget defaults selected by the evidence checkpoint.

## Structural removal inspections

After migration, scoped source searches must establish the required final structure. Exact searches are finalized with implementation symbols, including:

```text
rg -n "setDataType\(arg_types\[" src tests
rg -n "inferred_function_return_types_|record_inferred_function_return_type|inferred_function_return_type" src tests
rg -n "func_ret_to_sgtype|param_data_type" src/StyioLowering tests
rg -n "Undefined.*i64|fallback.*i64" src/StyioLowering src/StyioCodeGen tests
```

Expected results are no displaced implementation or positive test. New Sema modules contain the sole scheme/unifier/call-table authority; SGIR verifier paths require concrete signatures. Historical plan/decision evidence may name removed symbols and is not an executable compatibility route.

## Targeted commands

The validation-matrix checkpoint records the repository's active build directory and exact discovered test names. The final suite must include production/test target compilation plus focused CTest/gtest filters for parser, Sema, lowering, IR verifier, codegen/JIT, IDE, security, and end-to-end callable cases. It also runs the repository's current equivalents of:

```text
cmake --build <build-dir> --target styio styio_test styio_parser_internal_test styio_newparser_internal_test styio_ide_test
ctest --test-dir <build-dir> --output-on-failure -L styio_pipeline
ctest --test-dir <build-dir> --output-on-failure -L ide
ctest --test-dir <build-dir> --output-on-failure -L security
python scripts/syntax-convergence-gate.py
python scripts/docs-index.py --check
python scripts/docs-audit.py
python scripts/docs-lifecycle.py validate
python scripts/local-info-leak-gate.py --mode worktree
python scripts/manifest_tool.py validate docs/plan
```

Commands that are unavailable or renamed at execution time must be replaced with the discovered owning target and recorded in this file before acceptance; silently omitting a layer is not allowed.

## Final evidence record

The final-validation node records the Q02 and Q05 owner revisions, source head, build configuration, canonical budget values, commands/results, requirement IDs, scheme/instance fingerprints for order permutations, performance/growth metrics, CLI/IDE/cache comparison, deleted symbols/tests/docs list, and any unrelated pre-existing failure. Any source change after that record returns to the owning implementation node and reruns the complete mapped evidence.
