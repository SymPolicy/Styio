# Styio Unit Zero-Payload Boundaries — Evidence

**Purpose:** Record repository evidence and comparable language/runtime lessons that justify the frozen Unit boundary and its implementation constraints.

**Plan:** `styio-block-completion-and-bottom-type/unit-zero-payload-boundaries`

**Last updated:** 2026-07-14

## Repository evidence

| Surface | Current evidence | Required consequence |
|---|---|---|
| Canonical types | `src/StyioToken/Token.hpp` has no `unit`/`never` entries in `DTypeTable`; `src/StyioSession/TypeTable.hpp/.cpp` pre-registers `builtin_void` as `Undefined("undefined")`. | Add canonical Unit identity and keep compiler-invalid/undefined identity separate; layout is a distinct query. |
| Task typing | `src/StyioSema/TypeInfer.cpp:1710-1718` turns `task[unit]` into `kI64Type`; `src/StyioLowering/AstToStyioIR.cpp:3248-3254` repeats the rewrite. | Delete both repairs and preserve Unit once through Sema and IR. |
| Task runtime | `src/StyioExtern/ExternLib.cpp:187-192,260-349` supports only I64/F64/String tasks and always owns payload slots/function kinds. | Add a Unit execution kind/state path with no result payload and with lifecycle/failure published independently. |
| List runtime | `src/StyioExtern/ExternLib.cpp:53-148,2718-2744` stores one `std::vector<T>` per element family and derives length from `elems.size()`. | A Unit specialization must own `logical_len`; it cannot use a dummy vector, pointer movement, or byte size. |
| Dictionary runtime | `src/StyioExtern/ExternLib.cpp:476-543` stores ordered `pair<key,value>` entries plus an index and has no Unit value kind. | Preserve ordered keys/index as membership authority while omitting mapped Unit payload. |
| Native mapping | `src/StyioNative/NativeInterop.cpp:1306-1320` maps `CTypeKind::Void` to `Undefined`; `src/StyioCodeGen/CodeGenG.cpp:1974-1979` already lowers native void to LLVM `void`. | Adapt returning C `void` to semantic Unit while keeping LLVM no-result representation at the backend. |
| Tests that encode debt | `tests/typeinfer_internal_test.cpp:2101` expects `task[unit] -> i64`; `tests/security/styio_security_test.cpp:9636-9643` expects C `void -> Undefined`. | Replace these assertions with canonical Unit/boundary assertions in the same migration. |

## Comparable language and implementation experience

1. The [Rust tuple-type reference](https://doc.rust-lang.org/reference/types/tuple.html) defines `()` as a type with one value, while the [Rust layout reference](https://doc.rust-lang.org/reference/type-layout.html) permits zero size. This cleanly separates semantic cardinality from storage bytes.
2. Rust's [Result documentation](https://doc.rust-lang.org/std/result/) uses `Result<(), E>` for fallible work with no business payload. Unit denotes success; the error branch remains explicit.
3. The [Rust Nomicon zero-sized `Vec` chapter](https://doc.rust-lang.org/nomicon/vec/vec-zsts.html) documents real bugs from treating pointer movement as a zero-sized element count: allocation assumptions, division by zero, empty/infinite iterators, and alignment mistakes. Styio therefore uses logical counters and indices, never addresses, for Unit lists.
4. The [C++ object model](https://eel.is/c++draft/basic.types.general) makes `void` incomplete and forbids objects of that type. That design forces generic APIs to exclude/specialize `void`; Styio deliberately keeps Unit as the uniform source type and confines `void` to ABI adapters.
5. Rust's [FFI guidance](https://doc.rust-lang.org/nomicon/ffi.html) keeps source non-null references separate from C nullable pointers and performs optional conversion at the boundary. Styio likewise maps declared nullability into `? | T` instead of allowing null inside ordinary `T`.
6. LLVM's [void type reference](https://llvm.org/docs/LangRef.html#void-type) defines `void` as having no value and no size. It is suitable for a backend no-result convention, not as Styio's generic Unit value.
7. Clang's [nullability attributes](https://clang.llvm.org/docs/AttributeReference.html#nullability-attributes) distinguish nullable, non-null, and unspecified pointers. An unspecified C pointer therefore cannot justify silently constructing an ordinary Styio `T`.

## Failure lessons applied

- **Do not use fake payloads.** A dummy byte or integer preserves implementation convenience by corrupting type identity, overload selection, reflection, memory accounting, and task APIs.
- **Do not use pointer arithmetic for Unit cardinality.** Zero-sized addresses need not advance and cannot encode length or iterator progress.
- **Do not collapse states because payload is absent.** `? | unit`, an empty/non-empty Unit list, dictionary membership, task success/failure, and a returning/non-returning foreign call all remain distinct.
- **Do not let the ABI define the language.** LLVM `void`, C null, and `unreachable` are adapter facts; source typing remains `unit`, `? | T`, and `never`.
- **Do not leave duplicate repairs.** The existing Sema and lowering Unit-to-`i64` paths show how a workaround becomes inconsistent when copied across layers.

## Evidence gaps to close during implementation

1. Inventory every collection helper, intrinsic, iterator, clone, serializer, equality path, and resource transfer that switches on list/dictionary value kind.
2. Inventory task spawn/pull/codegen helpers and verify a Unit task cannot enter an integer ABI/helper by default.
3. Define explicit native metadata for nullability and no-return rather than inferring either from a pointer or `void` spelling alone; fail closed for absent/unspecified nullability.
4. Prove whether current TypeTable `bit_width` users assume that zero bits means `Undefined`; those consumers must use semantic identity instead.
5. Record memory and iteration evidence for large Unit lists and key-only Unit dictionaries without weakening observable behavior.
