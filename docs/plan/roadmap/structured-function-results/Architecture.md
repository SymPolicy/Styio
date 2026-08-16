# Structured Function Results

**Purpose:** Define the structured function-result type, IR, runtime, and ownership boundary.

**Last updated:** 2026-08-02

## Status and scope

This design activates the tuple syntax already accepted by the nightly parser as one immutable, structurally typed runtime value. The first complete slice supports tuple literals, local bindings, typed direct user-function returns, constant positional projection, and supported owned values nested in a tuple. It exists as a general language boundary; Brainfuck is only its first downstream consumer.

Named records, field mutation, dynamic heterogeneous indexing, tuple iteration, indirect-callable tuple results, tuple parameters, nested tuples, variadic callable signatures, native/extern tuple ABI, and compatibility with the old Brainfuck header protocol are outside this closure.

## Active source contract

Construction uses the existing parenthesized expression syntax. Return annotations use the existing tuple return-type syntax. Projection reuses the existing postfix index surface and requires a compile-time non-negative integer literal:

```styio
# compile : (i64, i64, list[i64]) := (source: list[char]) => {
    status = 0
    detail = 0
    bytecode: list[i64] := []
    <| (status, detail, bytecode)
}

result := compile(source)
status := result[0]
detail := result[1]
bytecode: list[i64] := result[2]
```

A tuple has a fixed ordered element shape. `(i64, i64, list[i64])` is distinct from `(i64, list[i64], i64)`. Tuple projection is zero-based. A nonliteral index, an out-of-range index, a tuple annotation whose arity differs from the returned value, or an element-type mismatch fails during semantic analysis. No dynamic fallback is generated.

Tuple literals must have at least two elements in this slice. Parenthesized single expressions remain grouping, and the parsed empty tuple remains unsupported as a runtime value until a concrete unit-value requirement exists.

## Type representation

`StyioDataTypeOption::Tuple` becomes a structural type rather than an unshaped marker. `StyioDataType` carries an immutable shared tuple-shape descriptor containing the ordered `StyioDataType` elements. The descriptor is compared structurally and recursively; absent shape metadata on a tuple type is invalid after Sema.

Sema performs these operations:

1. Infer every tuple literal element once and attach the ordered shape.
2. Convert `TypeTupleAST` into the same structural descriptor.
3. Validate every explicit return against the declared shape, including every reachable match/conditional return.
4. Preserve the shape through function-definition metadata, direct call inference, final/flexible bindings, and constant index projection.
5. Reject tuple values at unsupported boundaries before lowering.

This avoids name-keyed side tables that lose shape through rebinding and keeps tuple shape as ordinary type information.

## IR contract

Add two explicit value nodes:

- `SGTupleCreate(elements, element_types)` constructs one tuple value.
- `SGTupleGet(tuple, index, element_type)` projects one statically known element.

`SGType`, `SGFunc.ret_type`, `SGCall.result_type`, bind variables, and return nodes retain the tuple shape. The verifier requires:

- a shaped tuple type;
- equal element/value counts;
- supported, fully shaped element types;
- a constant in-range projection index;
- projection result type equal to the indexed shape element; and
- function return shape equal to the declared function result.

The optimizer and IR walker visit every tuple element and the projection source. They do not rewrite ownership or flatten the tuple into unrelated scalar statements.

## Runtime representation and ABI

The direct user-function ABI represents a tuple as one generation-checked `i64` handle, matching the established collection-handle calling convention. A tuple registry entry owns an immutable vector of tagged payloads. Supported element families are `i64`, `bool`, `char`, `f64`, `string`, `list`, `dict`, and `matrix`; only the Brainfuck-required scalar and `list[i64]` path is mandatory acceptance, while shared implementation helpers must fail closed for unsupported families.

The runtime exports bounded operations for create, clone, typed projection, release, and active-handle observation. Construction is O(k), projection is O(1), clone/release is O(k), and storage is O(k) for tuple arity `k`. Handles keep the existing slot/generation stale-handle checks.

LLVM functions returning a tuple therefore return `i64`. `toLLVMType` maps a fully shaped tuple result to `i64`; shape remains compile-time metadata on IR and is never reconstructed from the handle.

## Ownership contract

`TempResourceKind` and dynamic binding tags gain `Tuple`. One tuple registry entry owns every owned payload it contains.

- Scalar, bool, char, and float elements are copied into the entry.
- A string or collection temporary already owned by tuple construction is moved into the entry and removed from temporary cleanup.
- A borrowed string or collection element is cloned once for tuple ownership.
- Returning an owned tuple temporary or tuple local transfers its handle to the caller; returning a borrowed tuple would clone, though tuple parameters are not active in this slice.
- Projecting a scalar copies it. Projecting a string or collection clones that element and registers the clone as a caller-owned temporary. The tuple retains its own element until tuple release.
- Releasing a tuple releases each owned string/collection exactly once, invalidates the handle generation, then drops the entry.
- Every normal scope exit, early return, break, runtime-error guard, and task/codegen state isolation includes tuple temporaries in the same cleanup discipline as list/dict/matrix handles.

Projection cloning is intentionally chosen over borrowed element aliases: the projected `bytecode` remains valid after the temporary `result` tuple leaves scope, and ownership is locally decidable.

## Diagnostics

The compiler must use deterministic tuple-specific diagnostics for:

- runtime tuple literals with fewer than two elements;
- unshaped tuple types reaching Sema or IR;
- return arity mismatch;
- return element-type mismatch identifying the element index;
- nonliteral or out-of-range tuple projection;
- tuple use at an unsupported parameter, indirect-callable, native, extern, iterator, or mutation boundary; and
- invalid or stale tuple handles at runtime.

Existing tests that assert the blanket “tuple returns are not implemented” diagnostic are migrated completely to the active contract or a narrower unsupported-boundary diagnostic. No old blanket check remains.

## Ordered handoff

1. Compiler implementation establishes shaped tuple types, explicit IR, handle ABI, ownership, diagnostics, and focused tests. Its stable handoff is the source contract shown above plus passing nested-list lifetime evidence.
2. The downstream Brainfuck migration depends on that compiler handoff. It returns `(status, detail, bytecode)`, removes the two header cells, makes VM instruction addressing start at zero, and preserves the existing O(n) compiler and O(1) jump behavior.
3. Final validation reviews the complete capability once, confirms no status-prefix compatibility remains, then runs one impacted compiler regression and one downstream example acceptance.

The two implementation Nodes are not parallel: the example consumes a newly established compiler interface. Within the compiler Node, Sema/IR and runtime/codegen work are one ownership-coupled closure and must not be split between uncoordinated implementations.

## Risks and controls

- **Shape erasure:** prevented by structural type metadata carried through calls and binds; verified before codegen.
- **Nested handle leak or double release:** prevented by one tuple owner, move-on-construction, clone-on-projection, and active-handle baseline tests.
- **Use-after-release after projection:** prevented by returning an owned clone for string/collection projection.
- **ABI confusion with other `i64` handles:** prevented by generation-checked tuple registry operations and compile-time shaped IR.
- **Parser ambiguity:** limited to existing `(a, b)` literals and `:(T, U)` returns; projection reuses constant postfix indexing.
- **Scope expansion into records:** named fields remain explicitly out of scope; Brainfuck uses three named local projections at the consumer boundary.

## Design pattern assessment

```text
pattern_catalog: refactoring-guru-catalog-22-v1
candidate: none
decision: reject
pressure: Existing tuple syntax lacks one coherent type, IR, ABI, and nested-resource ownership path; the concrete pressure is representation and lifetime correctness across a direct function return.
expected_benefit: No catalog object pattern improves the required compiler invariant. The verifiable benefit comes from one structural type descriptor, two explicit IR nodes, and one generation-checked aggregate owner.
simpler_alternative: A direct immutable tuple value plus ordinary structural typing and handle operations is sufficient. A Brainfuck-only CompileResult object or integer protocol would be less general and would retain representation coupling.
application: Implement the direct value pipeline only at tuple construction, typed direct returns, constant projection, and nested resource cleanup; verify it with parser-through-runtime fixtures and active-handle baselines.
costs_and_rejections: Composite is rejected because tuples are not recursive trees in this slice; Builder is rejected because construction is atomic and ordered; Adapter/Facade would preserve an obsolete numeric protocol; Visitor is rejected because the existing compiler visitors already provide traversal and no new object hierarchy is justified.
```
