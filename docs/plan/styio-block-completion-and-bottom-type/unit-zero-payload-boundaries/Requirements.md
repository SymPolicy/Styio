# Styio Unit Zero-Payload Boundaries — Requirements

**Purpose:** Define the source-observable and representation-safety requirements for frozen decisions `O01-Q10..Q12`.

**Plan:** `styio-block-completion-and-bottom-type/unit-zero-payload-boundaries`

**Last updated:** 2026-07-14

## User problem

Styio already names `unit` as a task result, but Sema and lowering rewrite that result to `i64`; the runtime has no Unit collection/task kinds; native C `void` maps to compiler `Undefined`. These shortcuts confuse “requires no payload bytes” with “has no semantic value or state.” The failure is most dangerous in containers and asynchronous work: pointer movement or payload byte counts cannot represent logical cardinality, membership, lifecycle, or failure.

## Functional requirements

### REQ-UZ-001 — Unit has canonical semantic identity

`unit` is a first-class built-in type with exactly one value, `()`. Its canonical `TypeId` is distinct from `Undefined`, `never`, absence, C/LLVM `void`, and every integer type. It is accepted wherever the corresponding built-in generic constructor accepts an ordinary type argument.

### REQ-UZ-002 — Logical count/state and physical payload are separate

Every representation that contains or transports Unit has an authoritative logical field or control-flow fact for presence, count, membership, lifecycle, failure, and completion. Physical payload size, element allocation, pointer identity, address difference, and ABI return registers are never authoritative for those facts. Zero payload may remove bytes; it may not remove a state or change semantic cardinality.

This is a mandatory architecture and acceptance invariant. Implementations must reject count overflow, must not divide by a zero element size, and must not create fake one-byte/one-integer Unit payloads merely to reuse pointer-based algorithms.

### REQ-UZ-003 — Optional Unit has two states

`? | unit` has exactly two states: absent and present `()`. Presence is represented by an explicit discriminant or a separately proven valid niche; because Unit itself has no invalid payload pattern, payload bytes cannot encode presence. This must use the same canonical `? | T` representation as every other optional union; a Unit-only pseudo-option is forbidden. The representation must compose with the frozen normalization `? | (? | T) == ? | T` and expose no implicit unwrap or default.

### REQ-UZ-004 — Unit collections preserve cardinality and membership

`list[unit]` preserves logical length, indexing/bounds, slicing, cloning, mutation, equality where supported, and exactly-once iteration count while permitting zero element-payload storage. `dict[K,unit]` preserves keys, deterministic order, size, membership, lookup presence, cloning, and mutation while permitting mapped Unit payload storage to be omitted. Implementation covers the key families the dictionary runtime already supports (currently `string`) and must not expand key typing as a side effect. Public dictionary semantics do not become a separate set type.

For `list[unit]`, length and iterator progress are explicit integer metadata; each mutation uses checked arithmetic. For `dict[K,unit]`, the key/index structure is authoritative for cardinality and membership. Neither representation infers state from allocation or addresses.

### REQ-UZ-005 — Unit tasks and fallible completion preserve state

`task[unit]` records pending/running/success/failure/consumed facts independently of result payload storage. A successful pull/settlement supplies typed `()` exactly once. A failed operation exposes its typed failure/effect and supplies no success value. Unit is never rewritten to `i64`, zero, absence, or an internal sentinel.

A no-business-payload operation that can fail has Unit as its success payload and the language's typed effect/result channel as its failure path. D02 excludes an ordinary value fallback spelling; P01.14-A separately fixes `?| operation | fallback/handler` settlement, ordinary result binding, and generic `operation -> destination` composition without changing this representation requirement.

### REQ-UZ-006 — Foreign facts cross only explicit ABI adapters

A returning foreign C `void` function adapts to Styio `unit` after the call returns. A declared nullable foreign value adapts to `? | T`; an ordinary non-optional `T` rejects null. A proven no-return foreign call adapts to `never`. Source `unit` is not emitted as a C object or parameter; optional unions have no implicit C layout.

Native signature metadata keeps successful no-result, nullable result, and non-returning control flow distinct. A pointer whose nullability is absent or unspecified does not silently become an ordinary non-null Styio `T`; import fails closed until an explicit adapter contract supplies nullability. LLVM `void`, null pointers, and `unreachable` remain backend/ABI facts and never become source `Undefined` values.

### REQ-UZ-007 — Typed IR preserves zero-payload facts

StyioIR retains the canonical Unit type on expressions, Block results, task settlement, optional presence, and native adapters even when a node has no payload operand. Verifiers reject a Unit-to-integer repair, a value-producing use of `never`, a missing optional tag, or a native `void` mapped to `Undefined`. LLVM lowering erases payload only after these facts are proven.

### REQ-UZ-008 — One implementation and one migration

Compiler, runtime, native interop, tooling, docs, and tests converge in one migration. Duplicate Unit-to-`i64` rewrites, old payload-derived algorithms, `void`-to-`Undefined`, and positive fixtures that require them are removed. No legacy flag, compatibility AST, parallel runtime kind, or version-dependent source behavior remains.

## Non-functional constraints

1. Unit list length, indexing, push/pop/insert/remove, and iterator step are O(1); iterating `n` Unit values is O(n) observable events with O(1) element-payload storage.
2. Unit dictionary membership/lookup uses the existing ordered key/index strategy and retains its expected average complexity; the optimization removes mapped payload, not key/index authority.
3. Task state publication follows one release/acquire protocol so success/failure metadata is visible before settlement observes readiness.
4. Semantic identity is session-canonical through `TypeTable`; physical layout is queried separately and does not participate in value cardinality.
5. Error paths are fail-closed and source-located. No optimization level, target ABI, or allocator behavior changes a diagnostic or observable count/state.

## Scope and non-goals

Owned modules include `StyioToken`, `StyioSession`, `StyioSema`, `StyioIR`, `StyioLowering`, `StyioCodeGen`, `StyioExtern`, `StyioRuntime`, and `StyioNative`, plus affected tests and active language docs. General generics, arbitrary zero-sized user types, task cancellation, full effect syntax, resource cleanup aggregation, and record/tuple ABI layout remain outside this plan.

## Final acceptance target

All `REQ-UZ-*` labels have positive, negative, structural, runtime, and boundary evidence on one head commit. Search evidence finds no Unit-to-integer rewrite, no native-void-to-Undefined mapping, and no use of payload addresses/bytes as Unit collection cardinality. The optimized representations pass large-count, empty, mutation, clone, iteration, task failure, FFI, verifier, and documentation gates.
