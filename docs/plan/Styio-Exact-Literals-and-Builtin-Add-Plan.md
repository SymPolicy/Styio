# Styio Exact Literals and Built-in Add Plan

**Purpose:** Deliver Q05-LIT-ADD as bounded exact literal semantics, a closed scalar `Add` relation, checked signed-integer completion flow, and strict deterministic IEEE floating-point behavior, while deleting every conflicting coercion and backend-default path.

**Last updated:** 2026-07-20

**Status:** Pending implementation. Q05-LIT-ADD is accepted and every checkpoint remains pending.

**Better Plan ID:** `a0063a94-8e76-4121-937a-0a43fa94b8d1`

## 前置条件

1. **唯一语义所有者：** [Styio-Exact-Literals-and-Builtin-Add.md](../design/Styio-Exact-Literals-and-Builtin-Add.md) solely owns the exact-literal, materialization, defaulting, scalar `Add`, and completion semantics. This plan chooses implementation structures and evidence only.
2. **Q02 seam:** [Styio-Callable-Principal-Inference-Plan.md](./Styio-Callable-Principal-Inference-Plan.md) owns type variables, schemes, generalization, instantiation, and monomorphization. Q05 publishes a narrow immutable relation/catalog interface and conservative completion upper bound; it does not build a second inferencer.
3. **Completion infrastructure:** Existing operation-summary, settlement, and completion-CFG infrastructure remains owned by the operation-completion and directional-flow plans. Q05 contributes the payload-free `overflow` family and checked integer `Add` edge only.
4. **External semantics:** Accepted `Q03-F` owns strict operands, dependency/effect ordering, completion stop/publication, and optimizer rights through [Styio Functional Evaluation and Effect Ordering](../design/Styio-Functional-Evaluation-and-Effect-Ordering.md) and its dedicated implementation plan. Q06 owns text concatenation, later Q05/Q08 work owns container and matrix arithmetic, and F02 owns author-defined operator instances. Removal of current accidental `+` acceptance must conform to Q03-F without implementing it or deciding those remaining relations.
5. **One migration:** The production switch deletes the old promotion, string coercion, host parsing/folding, untyped IR repair, and backend default routes in the same converged change. No feature flag, compatibility visitor, or dual semantic catalog remains.
6. **并行：** Evidence, validation fixtures, and file-disjoint numeric/IDE lanes may run in parallel after interfaces are frozen. Shared Sema, SGIR, and backend paths are serialized through the checkpoint graph.
7. **子智能体：** Sub-agents may handle read-only research or file-disjoint delivery lanes, but one coordinator owns shared numeric interfaces, cross-plan ownership checks, and final convergence.
8. **基座：** Reuse the repository's existing parser, compilation session, type table, completion CFG, StyioIR verifier, LLVM backend, diagnostics, IDE bridge, cache, test, and documentation infrastructure; general-purpose substrate changes belong to their existing foundation owners rather than a Q05-local replacement.

## Delivery target

The compiler preserves integer literals as arbitrary-precision signed values and decimal literals as exact signed coefficient/base-ten-exponent values until a closed context selects `i8`, `i16`, `i32`, `i64`, `i128`, `f32`, or `f64`. Deterministic token, digit, bit, exponent, and work budgets reject excessive input before disproportionate allocation or computation without truncating or changing its kind.

Materialization is failure-closed, defaulting occurs only at mandatory concrete ordinary-value boundaries, and generalizable callables retain Q02 constraints without defaulting. One immutable finite `Add` catalog admits identical concrete scalar operands and materializable exact literals only. Its generalized completion upper bound is conservatively `{overflow}`.

Selected signed-integer rows use checked addition and direct `overflow` completion control flow. Selected floating rows use strict IEEE round-to-nearest-ties-to-even behavior with gradual underflow, subnormals, and signed zero preserved. Constant evaluation and runtime lowering consume the same row and operation descriptor.

## Scope

1. Exact literal parsing/canonicalization, semantic side-table storage, and deterministic resource accounting.
2. Closed materialization/default boundaries and precise failure classifications.
3. One finite scalar `Add` relation plus the narrow Q02 query/fingerprint/completion-bound interface.
4. Shared constant evaluation, concrete SGIR operation summaries, checked integer lowering, and strict floating lowering.
5. Concrete LLVM type mapping for all admitted widths/formats and target capability verification.
6. Stable diagnostics, IDE semantic facts, incremental cache identity, tests, performance/memory evidence, documentation migration, and structural deletion gates.

## Non-goals

- No new numeric literal spelling, radix, separator, exponent syntax, keyword, type, or explicit conversion syntax.
- No unsigned/platform integers, wrapping or saturating arithmetic, subtraction/multiplication/division semantics, NaN comparison/payload policy, or configurable overflow mode.
- No text, container, matrix, byte, character, or boolean `Add` row; no user-defined/open `Add` relation.
- No duplicate ownership of Q02 schemes/solver/specialization, generic settlement/CFG mechanics, accepted Q03-F evaluation/effect ordering, or deferred collection/operator policy.

## Execution graph

The detailed all-pending graph and delivery contracts are under [styio-exact-literals-and-builtin-add](./styio-exact-literals-and-builtin-add/Plan.md):

- [Requirements.md](./styio-exact-literals-and-builtin-add/Requirements.md)
- [Evidence.md](./styio-exact-literals-and-builtin-add/Evidence.md)
- [Architecture.md](./styio-exact-literals-and-builtin-add/Architecture.md)
- [Validation.md](./styio-exact-literals-and-builtin-add/Validation.md)
- [Checkpoints.json](./styio-exact-literals-and-builtin-add/Checkpoints.json)

## 验收条件

1. Every `REQ-LITADD-*` label maps to implementation checkpoints and executable or structural evidence on one head commit.
2. Exact integer/decimal representation and every materialization/default boundary match the sole semantic owner, including integer `-0`, decimal `-0.0`, exact integer-to-float conversion, ties-to-even decimal conversion, and finite-to-infinity rejection.
3. The complete closed `Add` table is tested symmetrically; concrete mismatches and excluded families reject without promotions, string concatenation, or backend inference.
4. No defaulting occurs during Q02 generalization, and the stable generalized `Add` completion upper bound remains `{overflow}` even for a later floating instantiation.
5. Every signed width uses checked addition with the nominal payload-free `overflow` edge; compile-time and runtime outcomes agree exactly.
6. `f32`/`f64` lowering proves strict IEEE behavior across supported optimization modes and rejects unsupported targets instead of enabling fast-math, FTZ/DAZ, reassociation, or host-rounding dependence.
7. CLI, LSP/IDE, caches, tests, documentation, performance/memory receipts, and structural searches all converge on the single new pipeline, with obsolete implementation and fixtures removed.
