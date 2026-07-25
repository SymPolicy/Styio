# Styio Unit Zero-Payload Boundaries — Architecture

**Purpose:** Define the semantic/layout firewall, specialized zero-payload representations, algorithms, and compiler/runtime/ABI ownership boundaries.

**Plan:** `styio-block-completion-and-bottom-type/unit-zero-payload-boundaries`

**Last updated:** 2026-07-14

## 1. Mandatory firewall

Two domains remain distinct across every layer:

| Semantic domain — authoritative | Physical domain — optimization only |
|---|---|
| canonical `TypeId` and value cardinality | payload size/alignment |
| optional presence | tag packing/niche/layout |
| list length and iterator index | element buffer/allocation/address |
| dictionary key membership and order | mapped-value storage |
| task lifecycle, success/failure, consumption | result payload slot/register |
| Block completion and divergence | LLVM operand/return representation |
| foreign nullability/no-return classification | C pointer/`void` ABI spelling |

No interface may reconstruct a left-column fact from the right column. In particular, zero payload size never means undefined, absent, empty, failed, not completed, or diverging.

## 2. Type and layout boundary

`TypeTable` owns semantic identity. It registers distinct canonical IDs for Unit, Never, and the invalid/Undefined sentinel. A layout service keyed by canonical `TypeId` returns a descriptor equivalent to:

```text
TypeLayout = {
  payload_size: usize,
  payload_align: usize,
  has_payload: bool
}
```

For Unit, `payload_size = 0`, `payload_align = 1`, and `has_payload = false`; these fields do not participate in semantic equality. `TypeKey::bit_width == 0` is not sufficient to recognize Unit because strings, handles, compiler-invalid types, and target-specific values may also report zero inline bits. The exact C++ type name may follow existing style, but an equivalent semantic/layout separation is mandatory.

## 3. Representation matrix

| Source fact | Logical representation | Physical payload representation |
|---|---|---|
| `()` | canonical Unit `TypeId` / typed Unit IR fact | no payload operand |
| `? | unit` | canonical optional-union `is_present` discriminant | no Unit payload bytes |
| `list[unit]` | `logical_len` and iterator `{index,end}` | no per-element buffer |
| `dict[K,unit]` | ordered keys plus key-to-index/member structure | no mapped Unit field |
| `task[unit]` | atomic lifecycle plus typed failure and consumed state | no result payload slot; Unit callback returns no value |
| returning C `void` | adapter marks successful-return-to-Unit | LLVM/C no-result call |
| nullable foreign `T` | adapter nullability plus optional tag | target pointer/value payload |
| foreign no-return | adapter marks `never` edge | LLVM `unreachable` after a proven no-return call |

## 4. Unit list algorithms

The Unit specialization owns `logical_len: size_t`; capacity may be tracked separately for uniform diagnostics/limits but is never inferred from an allocation. Operations are defined on count, not payload:

- `len` reads `logical_len` in O(1);
- `push`/`insert` check `logical_len + 1` for overflow and then increment;
- `pop`/`remove` validate non-empty/index and then decrement;
- `get` validates `index < logical_len` and returns typed `()` without loading bytes;
- `set` validates the index and Unit RHS, then performs no payload write;
- `slice(a,b)` validates bounds and creates a Unit list with logical length `b-a`;
- `clone` copies logical length; equality, where list equality exists, compares length;
- iteration keeps an integer index/end and emits exactly `logical_len` Unit events.

Forbidden implementations include pointer-difference length, `capacity * sizeof(unit)`, division by element size, repeated identical-address stepping, fake allocation, and a vector of integer/byte placeholders. Large-count behavior is limited by checked logical arithmetic, not address space for Unit payloads.

## 5. Unit dictionary algorithms

The current ordered dictionary model keeps deterministic entry order plus an index. For each already supported key family (currently `string`), the Unit specialization retains keys/order and the key index, but the entry shape contains no mapped Unit payload. Size is the key count. Lookup checks membership and constructs present `()` or absence at the language boundary. Insert/update/remove/clone/values-iteration preserve dictionary semantics; values iteration emits one `()` per key. A set-like physical layout is allowed, but the public type and lookup result remain `dict[K,unit]` and `? | unit`; this work does not expand dictionary key support.

## 6. Unit task and effect state

The runtime separates a common task control block (lifecycle, failure, context ownership, consumption, profiling) from typed result payload storage. Payload-bearing task specializations own only their result type; the Unit specialization owns no result field and uses a no-result callback. One atomic state machine (`pending -> running -> succeeded|failed -> consumed`) replaces any integer payload sentinel. Failure metadata is written before a release publication of the terminal state; settlement uses acquire observation before reading failure details. Successful settlement constructs a typed Unit fact; failed settlement exposes the typed effect and does not construct Unit. A `variant<monostate,...>` that still reserves the maximum payload in every Unit task does not satisfy the physical-payload requirement.

D02 now excludes every ordinary value-level recovery operator; P01.14-A separately freezes generic directional flow and operation settlement with ordinary result binding. Internally, Unit success and typed failure must remain disjoint so settlement work cannot depend on an `i64` convention or a task-specific target binder.

## 7. IR and backend

StyioIR may represent Unit as a zero-operand typed constant/result, but every value edge keeps the canonical Unit `TypeId`. Optional Unit keeps its presence tag; list/dictionary/task handles keep their logical metadata/state. Verifiers reject integer substitution and missing tags.

LLVM lowering uses `void` only where LLVM requires a no-result function/call. A source function returning Unit may lower through the target no-result convention, but the frontend/Sema/StyioIR signature remains Unit. A Unit value never becomes `i64 0`; a `never` edge never manufactures Unit.

## 8. Native ABI adapters

Native signature metadata separates three attributes:

1. returns normally with no payload (`void` -> Unit);
2. returns a declared nullable value (`nullable T` -> `? | T`);
3. does not return (`noreturn` -> Never).

The adapter constructs the Styio semantic result after the call boundary. Source Unit is not passed as a C object. Exported Unit return uses the target no-result convention and explicit signature adaptation. Optional unions require explicit per-ABI adapters; they acquire no implicit C layout. Missing or unspecified pointer nullability fails closed until an explicit adapter declares nullable or non-null behavior.

## 9. Module ownership and serial implementation chain

| Stage | Primary modules/files | Consumes |
|---|---|---|
| Semantic/layout foundation | `StyioToken`, `StyioSession/TypeTable`, shared type/layout helpers, StyioIR type facts/verifier | parent Unit/Never contract |
| Collections | `StyioExtern/ExternLib*`, list/dict codegen helpers and collection tests | canonical Unit ID/layout query |
| Tasks/effects | task portions of `StyioSema`, `StyioLowering`, `StyioExtern`, task codegen/tests | canonical Unit ID/layout query |
| Native adapters | `StyioNative`, native-call Sema/codegen, FFI tests | canonical Unit/Never ID/layout query |
| Convergence | active docs, tooling mirrors, fixtures, obsolete-test deletion | all implementation lanes |

These stages are serial because `ExternLib.cpp/.hpp`, `CodeGenG.cpp`, JIT symbol registration, and value-family dispatch overlap. Read-only audits and file-disjoint tests may still run in parallel, but the checkpoint graph represents one serial implementation merge chain.

## 10. Migration and forbidden residue

The migration deletes the Sema and lowering Unit-to-`i64` branches, the native `void`-to-`Undefined` mapping, and tests that assert them. It also deletes any placeholder Unit element/task payload introduced during implementation. No legacy flag or fallback survives. Search-based structural tests guard the forbidden patterns, while executable tests prove the new semantics.
