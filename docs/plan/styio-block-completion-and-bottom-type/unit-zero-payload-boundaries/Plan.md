# Styio Unit Zero-Payload Boundaries Plan

**Purpose:** Deliver the frozen `unit` generic, collection, task/effect, and explicit FFI/ABI contract while making logical count and state structurally independent of physical payload storage.

**Last updated:** 2026-07-14

**Status:** Pending implementation. Owner decision `O01-Q10..Q12` is frozen as option A with the mandatory invariant “逻辑计数与物理载荷分离”.

## 前置条件

1. **并行：** Repository evidence, external-language evidence, test discovery, and documentation audits may run in parallel. Implementation is serial after architecture because collection, task, FFI, codegen, JIT registration, and value-family dispatch overlap in shared files; only read-only or proven file-disjoint work may remain parallel.
2. **子智能体：** Sub-agents may perform read-only audits and disjoint test/documentation work. One coordinator owns the serial semantic/layout, collection, task/effect, ABI, and final-SSOT merge chain.
3. **基座：** The parent [Styio Block Completion and Bottom Type Plan](../../Styio-Block-Completion-and-Bottom-Type-Plan.md) must supply canonical `unit`/`never` identities and typed Block results. Shared test or workflow substrate belongs to `styio-common-foundation`; this child must not create a second harness.
4. The frozen owner contract is [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../../../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md), especially `O01-Q02`, `O01-Q06..Q07`, and `O01-Q10..Q12`.
5. D02 excludes ordinary value recovery (`a | b` and `a ?? b`); P01.14-A separately freezes generic directional flow, operation settlement, and ordinary result binding. This plan implements the accepted type/state boundary without inventing a `Result` keyword, constructor family, unwrap, fallback operator, task-specific binder, or new token.

## Delivery target

The implementation has one enforceable firewall:

> Semantic cardinality, presence, membership, lifecycle, failure, and control-flow state are authoritative logical facts. Payload size, allocation, pointer identity, and ABI return registers are physical representation facts. No code may derive the former from the latter.

That rule produces these observable results:

- `unit` is a normal generic argument with the unique value `()` and a distinct canonical type identity;
- `? | unit` has absent and present-`()` states even though the present payload has zero bytes;
- `list[unit]` stores logical length/iteration progress independently of element payload, and `dict[K,unit]` stores membership independently of mapped payload;
- `task[unit]` stores lifecycle and typed failure independently of a result payload and settles successfully with `()` rather than `i64`;
- successful C `void`, nullable foreign values, and non-returning calls map only at explicit adapters to `unit`, `? | T`, and `never`;
- the source type system never exposes C `void`, null, internal `Undefined`, or a fabricated integer as a Unit value.

## Scope

1. Canonical Unit type identity and a semantic-type-to-physical-layout query boundary.
2. Zero-payload optional presence through the uniform `? | T` representation, Unit list cardinality/iteration, and Unit dictionary membership for currently supported key families.
3. Unit task execution/settlement and the typed failure channel for no-business-payload work.
4. Native signature adapters for `void`, declared nullable values, and proven no-return calls.
5. StyioIR/LLVM preservation of typed Unit facts while erasing payload only where legal.
6. One-shot deletion of Unit-to-`i64`, `void`-to-`Undefined`, payload-derived count/state paths, and tests that require them.
7. Language SSOT, diagnostics, feature/runtime/typeinfer/codegen/security tests, convergence evidence, and final gates.

## Non-goals

- No general user-defined zero-sized-type feature or arbitrary layout DSL.
- No promise that optional unions, Unit values, or Styio records have a C-compatible object layout.
- No C parameter object for source `unit`; a no-argument/no-result ABI adapter is a boundary operation, not a Unit object representation.
- No reopening of decided D02 or P01.14-A, no task-specific settlement binder, and no public `Result` constructor.
- No general user-defined generics, dictionary key-model expansion, cancellation policy, or resource-family cleanup policy.
- No compatibility execution path, integer Unit surrogate, or retained positive test for rejected behavior.

## Execution graph

The machine-validated graph is [Checkpoints.json](./Checkpoints.json). Delivery contracts are:

- [Requirements.md](./Requirements.md)
- [Evidence.md](./Evidence.md)
- [Validation.md](./Validation.md)
- [Architecture.md](./Architecture.md)

## 验收条件

1. Every `REQ-UZ-*` requirement maps to executable evidence in `Validation.md` and to implementation/final-validation nodes in `Checkpoints.json`.
2. Unit, optional presence, collection count/membership, task lifecycle/failure, and ABI classification each have an explicit logical owner that remains valid when payload size is zero.
3. A million-element `list[unit]` reports and iterates one million items without a million Unit payload objects; its count is never computed from pointer difference or byte size.
4. `dict[K,unit]` preserves insertion order/key membership and optional lookup semantics while allocating no mapped Unit payload per key.
5. `task[unit]` and returning C `void` produce typed `()` without an `i64`, null, or `Undefined` surrogate; nullable and no-return foreign signatures remain distinct.
6. Unit-specific paths have checked count arithmetic, deterministic bounds behavior, and no zero-size division, fake allocation, pointer stepping, or address-as-counter logic.
7. Old Sema/lowering/runtime/native mappings and their positive tests are removed in the same migration; only current negative diagnostics remain.
8. Targeted tests plus syntax-convergence, documentation, lifecycle, local-information, and Better Plan validation pass on one head commit.
