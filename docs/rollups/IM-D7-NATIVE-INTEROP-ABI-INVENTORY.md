# IM-D7 Native Interop ABI Inventory

**Purpose:** Record the accepted native interop ABI decisions for IM-D7 so `@ extern(c|c++)` is governed by a stable source, signature, symbol, and function-pointer contract.

**Last updated:** 2026-05-31

## Scope

IM-D7 owns the contract for Styio calling native code:

- native source block and referenced-source handling,
- host C/C++ compiler delegation,
- explicit Styio binding names,
- native signature extraction,
- ABI type mapping,
- native artifact loading,
- symbol resolution,
- function-pointer invocation,
- native diagnostic families,
- host toolchain/cache evidence, and
- the boundary between native implementation power and Styio language guarantees.

IM-D7 does not define the Styio package manager lifecycle, registry trust, vendoring, or install UX. Those remain IM-D10 and `styio-spio` concerns. IM-D7 also does not redefine IM-D5's stream determinism model; it only states how native calls enter that model as native effects.

## Current State

The preferred authoring surface is explicit binding syntax:

```styio
# inline_add := @ extern(c) {
    int inline_add(int a, int b) { return a + b; }
}

# ref_square := @ extern(c++) { "native/ref_square.cpp" }

# ref_square_1, ref_square_2 := @ extern(c++) { "native/ref_pair.cpp" }
```

Compatibility coverage still exists for legacy parser forms, but new feature fixtures, artifact tests, and examples use `# name[, other] := @ extern(...) { ... }`.

The current compiler already treats inline bodies and referenced source files as native source material, compiles them through a host compiler, loads a native artifact, resolves exported symbols, and registers callable addresses with the JIT/runtime.

## Accepted Native Model

Styio is a native-source orchestrator, not a replacement C or C++ frontend.

Accepted decision:

- The contents of `@ extern(c) { ... }` are C source material for a C compiler.
- The contents of `@ extern(c++) { ... }` are C++ source material for a C++ compiler.
- Referenced paths inside an extern block are native source inputs for the same host compiler route.
- Styio does not type-check full C or C++ bodies as Styio code.
- Styio does not reimplement header, macro, include, namespace, template, or overload semantics when the host compiler already owns them.
- Styio owns orchestration: source collection, compiler invocation, artifact loading, signature extraction, symbol resolution, function-pointer registration, and diagnostics.
- Explicit binding names are the Styio-visible callable surface. Native functions not named in the binding list are hidden from Styio source.

This keeps native interop aligned with the original design intent: use C/C++ and the Linux/native ecosystem where those tools are simpler, faster, or closer to the operating system than re-expressing the same work in Styio.

## Support Posture

IM-D7 is capability-first and fail-closed.

Accepted decision:

- Styio should support native forms when it can extract a callable signature, compile the source, load the artifact, resolve a unique symbol, and cast the symbol address to a safe function pointer shape.
- Styio should not reject useful native forms merely because they are C++ or because they use host-language features internally.
- Styio must reject forms where it cannot prove the exported callable's signature, symbol, or ABI mapping.
- Unsupported or ambiguous native forms fail with typed diagnostics instead of fallback guesses.

The design target is not "only support C ABI." The design target is "support what can be made stable; reject what cannot yet be made stable."

## Binding And Visibility

Accepted decision:

- `# name := @ extern(abi) { ... }` binds exactly one Styio-visible native callable.
- `# name1, name2 := @ extern(abi) { ... }` binds multiple Styio-visible native callables from the same native source material.
- Every binding name must correspond to exactly one native callable signature.
- Every bound callable must be resolvable from the loaded native artifact.
- Unbound native functions may exist in the same source material, but they are not visible as Styio functions.
- Missing bound functions, duplicate/ambiguous candidates, hidden-symbol calls, and symbol-resolution failures are native interop errors.

The binding list is not an export declaration for C/C++. It is the Styio-visible import surface from the compiled native artifact.

## Signature Extraction

Native signature extraction is mandatory.

Accepted decision:

- Styio must extract the native callable signature before registering a function pointer.
- Inline extern blocks and referenced source files follow the same rule.
- The extracted signature is mapped into Styio's type system before type checking native calls.
- Styio must not infer a native function type only from Styio call sites.
- Styio must not default an unknown native function to a placeholder signature.
- If the signature cannot be extracted or mapped, the binding fails before runtime invocation.

This is required because native dynamic symbol lookup returns an untyped address. The C/C++ compiler has strong type information during native compilation, but the load boundary exposes a raw symbol address. Styio must reconnect that native type information to a typed function-pointer call.

## ABI Type Surface

Accepted baseline:

| Native type family | Styio mapping rule |
|--------------------|--------------------|
| `void` return | statement/unit-like result; not a value-producing expression unless a later language rule defines unit values |
| `bool` / `_Bool` | `bool` |
| fixed-width and ordinary integer types | integer family, preserving width when the compiler surface can represent it |
| `float` / `double` | floating family, preserving width when the compiler surface can represent it |
| pointer types | accepted only through declared pointer/string/opaque-handle mappings |
| variadic functions | unsupported until Styio has a stable variadic call contract |
| structs/classes by value | unsupported until layout, ownership, and ABI rules are explicit |
| references, templates, overloaded functions | supported only when the exported callable resolves to one unambiguous function-pointer signature |

Implementation may support a narrower subset at any checkpoint, but each unsupported type family must fail with a stable native diagnostic instead of being coerced to an unrelated Styio type.

## C Priority And Linux Native Use

Accepted decision:

- C interop is the priority path for low-level Linux/native integration.
- Styio should make it practical to call small C functions, POSIX/Linux wrappers, and system-near helpers with minimal overhead.
- C can be a simpler and faster boundary than re-expressing the same operation in Styio when the operation is already native or operating-system-facing.
- C source can include headers, macros, and platform APIs through the host compiler route.
- Linux x86_64 is the first required native interop platform under IM-D6; other platforms remain tracked through the release matrix.

This does not make Styio a C replacement. It makes Styio able to use C where C is the right native interface.

## C++ Support Policy

Accepted decision:

- `extern(c++)` means the source is compiled by a C++ compiler.
- C++ internals may use namespaces, classes, templates, overloads, RAII, STL, or other C++ mechanisms when the host compiler accepts them.
- Styio only needs a final callable export with an extractable signature and resolvable symbol.
- A C ABI wrapper such as `extern "C"` is the most portable and preferred way to expose a C++ implementation to Styio, but it is not the design's only possible support path.
- Direct C++ symbol support may be accepted when the compiler can prove a unique exported callable, recover its signature, resolve the emitted symbol, and keep the result stable for the current toolchain/platform record.
- Ambiguous overloads, unstable mangled-name guessing, toolchain-dependent symbol selection, and missing signatures must fail closed.

Portable C++ example:

```cpp
namespace native {
int square_impl(int x) {
    return x * x;
}
}

extern "C" int ref_square(int x) {
    return native::square_impl(x);
}
```

Styio may later add explicit symbol mapping for advanced C++ forms, but v1 does not require users to accept C++ ABI instability as a language guarantee.

## Symbol Resolution

Accepted decision:

- The compiled native artifact is loaded through the platform's native loader path.
- Each explicit binding name must resolve to an executable callable address, or to an explicit symbol mapping when such a mapping is later added.
- Symbol lookup result plus extracted signature determines the function pointer type.
- Styio may use C ABI names, toolchain-assisted symbol lookup, or future explicit symbol mapping when the result is unique and documented.
- Styio must not silently choose between multiple possible native symbols.

Current implementation paths that rely on unmangled symbol names remain valid as the portable baseline.

## Toolchain And Cache Evidence

Accepted decision:

Native interop diagnostics and artifacts should record enough evidence to reproduce the native boundary:

- normalized ABI: `c` or `c++`,
- compiler command and source of compiler resolution,
- compile flags owned by Styio,
- source text or source hash,
- referenced source paths,
- cache key or artifact identity,
- load path,
- exported binding names,
- extracted signatures, and
- compile/load/symbol/signature failure category.

This is the native equivalent of a compiler contract record. It lets IM-D6 release evidence state exactly which native toolchain path was tested.

## Native Effects

Accepted decision:

- Native calls are not assumed pure.
- A native binding is host-effecting unless a later effect contract marks it otherwise.
- Native calls inside deterministic stream frames must be classified consistently with IM-D5.
- Host-effecting native code may read global state, perform I/O, allocate memory, start threads, or interact with Linux/POSIX APIs.
- Styio must not optimize a native call across resource, stream, or commit boundaries unless the native effect classification permits it.

This policy lets C remain useful for system-level operations without pretending those operations are ordinary pure Styio expressions.

## Diagnostics

Required native diagnostic families:

| Family | Meaning |
|--------|---------|
| unsupported ABI | ABI value is not accepted, such as `rust` or `cpp` when only `c` and `c++` are valid |
| source read failure | referenced native source cannot be read |
| signature not found | explicit binding has no extractable native signature |
| unsupported signature | signature exists but uses unsupported ABI types or variadic/aggregate forms |
| host compile failed | C/C++ compiler rejected the native source |
| load failed | native artifact could not be loaded |
| symbol missing | explicit binding could not be resolved from the loaded artifact |
| hidden symbol call | Styio source called a native function not present in the binding list |
| call ABI mismatch | extracted signature and call lowering cannot agree on a function-pointer shape |

Diagnostics should be stable enough for tests and services to classify native interop failures without string-matching host compiler logs.

Current public JSONL coverage maps missing referenced native sources to `STYIO_NATIVE_SOURCE_READ_FAILED`, explicit binding/signature misses to `STYIO_NATIVE_SIGNATURE_NOT_FOUND`, unsupported native signature shapes such as aggregate parameters or variadic signatures to `STYIO_NATIVE_UNSUPPORTED_SIGNATURE`, host compiler rejections to `STYIO_NATIVE_HOST_COMPILE_FAILED`, native artifact load failures to `STYIO_NATIVE_LOAD_FAILED`, missing exported symbols to `STYIO_NATIVE_SYMBOL_MISSING`, and unavailable toolchain configuration to `STYIO_NATIVE_TOOLCHAIN_UNAVAILABLE` while keeping the existing TypeError exit family for the sema/codegen route. The shared diagnostic taxonomy also reserves native interop families for unsupported ABI where the native layer owns it and the fallback `STYIO_NATIVE_INTEROP_ERROR`; these are diagnostic classifications only and do not broaden native ABI support, signature support, artifact loading, symbol visibility, C++ symbol mapping, or host toolchain behavior.

## Fixture Requirements

Every accepted native interop fixture should state or prove:

1. ABI: `c` or `c++`,
2. inline or referenced source route,
3. explicit binding list,
4. extracted native signatures,
5. expected Styio-visible functions,
6. expected hidden functions,
7. expected runtime output or artifact behavior,
8. expected diagnostic for negative cases, and
9. whether the fixture requires Linux/native toolchain support.

Feature fixtures should use the preferred explicit binding syntax. Legacy forms remain compatibility/parser coverage only.

## Stop Condition

IM-D7 can close only when:

1. explicit binding syntax is the primary documented native authoring surface,
2. each bound native function has extractable signature evidence,
3. type mapping accepts the documented baseline and rejects unsupported families with diagnostics,
4. inline and referenced source routes use the same signature/compile/load/symbol rules,
5. C and C++ host compiler paths both have positive and negative tests,
6. C++ portable wrapper behavior is documented and tested,
7. hidden native symbols remain invisible to Styio calls,
8. native build artifacts include enough toolchain/cache evidence for release records,
9. JIT and `styio build` artifact paths expose the same bound functions, and
10. unsupported native forms fail closed instead of guessing symbols, signatures, or calling conventions.

## Decision Closure

No IM-D7 design decision remains open in this inventory. Remaining work is implementation and test closure: broaden supported native signatures where the compiler can prove them, keep C/POSIX/Linux interop practical, add toolchain evidence to release records, and fail closed for native forms whose symbol or signature cannot be proven.

## Source Documents

- [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md)
- [Codegen / Runtime Runbook](../teams/CODEGEN-RUNTIME-RUNBOOK.md)
- [Frontend Runbook](../teams/FRONTEND-RUNBOOK.md)
- [Test Quality Runbook](../teams/TEST-QUALITY-RUNBOOK.md)
- [IM-D5 Stream Concurrency Inventory](./IM-D5-STREAM-CONCURRENCY-INVENTORY.md)
- [IM-D6 Release Conformance Inventory](./IM-D6-RELEASE-CONFORMANCE-INVENTORY.md)
