# Styio Compiler Intrinsics and Standard-Library Boundary

**Purpose:** Record compiler-owned intrinsic surfaces with implementation evidence, and keep deferred algorithm ideas separate from standard-library promises.

**Last updated:** 2026-06-25

---

## Scope

This document governs capabilities that need compiler ownership for parsing, type inference,
lowering, runtime helper routing, pulse-state storage, or code generation.

Compiler intrinsics are not automatically standard-library APIs. A capability may be active as a
compiler intrinsic while `library/manifest.json` still records only `std.resource` as an active
standard-library compatibility module. Planned standard-library modules remain reservations until
they have source, tests, diagnostics, and manifest evidence.

## Evidence Rules

An intrinsic entry is active only when it names current implementation evidence:

1. accepted source or call surface,
2. type/Sema behavior,
3. lowering or runtime route,
4. codegen or runtime helper behavior,
5. positive execution coverage,
6. negative diagnostic coverage when adjacent unsupported forms exist, and
7. `workflows/TEST-CATALOG.md` evidence for the maintaining gate.

Performance wording is allowed only when a measured benchmark or codegen-shape test is recorded.

## Active Intrinsics

### Series Intrinsics

| Surface | Current status | Evidence | Limits |
|---------|----------------|----------|--------|
| `series[avg, n]` | Active compiler intrinsic for the pulse/state path | Parser recognizes `avg` in `src/StyioParser/Parser.cpp`; AST is `SeriesIntrinsicAST`; lowering emits `SGSeriesAvgStep` in `src/StyioLowering/AstToStyioIR.cpp`; codegen is in `src/StyioCodeGen/CodeGenPulse.cpp`; parser, lowering, ownership, representation, and codegen tests live in `tests/security/styio_security_test.cpp`. | `n` must be an integer literal. Lowering requires an enclosing state slot. Current codegen is the i64 pulse-ledger route and stores explicit definition tags beside the current value and history; cold start or skipped absent input never consumes an `i64` bit pattern as a sentinel. |
| `series[max, n]` | Active compiler intrinsic for the pulse/state path | Parser recognizes `max`; lowering emits `SGSeriesMaxStep`; codegen is in `src/StyioCodeGen/CodeGenPulse.cpp`; focused tests cover parser AST, state-slot classification, IR lowering, and codegen shape. | Same pulse/state limits as `avg`. The current codegen scans the bounded ring for the current window; do not document a monotonic-deque guarantee without new evidence. |

Deferred series operators: `[min, n]`, `[std, n]`, `[ema, n]`, `[rsi, n]`,
`cross_over`, and `cross_under` are not active compiler contracts in this
checkout. They require the full evidence list above before being described as
accepted behavior.

### Matrix Intrinsics

Matrix helpers are ordinary call syntax at the grammar level and compiler-owned during Sema,
lowering, and runtime dispatch.

| Surface | Current behavior | Evidence |
|---------|------------------|----------|
| `mat_zeros`, `mat_zeros_i64` | Construct f64 or i64 matrices from integer dimensions. Static positive dimensions are preserved in the inferred type. | `infer_matrix_intrinsic_type` in `src/StyioSema/TypeInfer.cpp`; lowering/runtime routing in `src/StyioLowering/AstToStyioIR.cpp`; sample execution in `StyioSamples.MatrixOperationsAndIntrinsics`. |
| `mat_identity`, `mat_identity_i64` | Construct square identity matrices. Static positive size is preserved in the inferred type. | Same matrix-intrinsic Sema/lowering/runtime evidence. |
| `mat_shape`, `mat_rows`, `mat_cols` | Return shape metadata as `list[i64]` or `i64`. | Type and lowering tests in `StyioTypeInferenceContract.MatrixIntrinsicCallsInferShapesAndRejectBadInputs` and `StyioIRContract.MatrixIntrinsicLoweringUsesTypedRuntimeEntryPoints`. |
| `mat_get`, `mat_set`, `mat_clone` | Access, mutate, and clone matrix handles. `mat_set` returns `i64` status and checks element compatibility before lowering. | Runtime and sample tests cover i64/f64 operations, clone independence, and stable error boundaries. |
| `mat_add`, `mat_sub`, `mat_scale`, `mat_hadamard`, `matmul`, `transpose`, `dot`, `mat_sum`, `norm` | Numeric matrix arithmetic, transforms, and reductions with static shape checks where dimensions are known. | Sema shape checks, lowering entrypoint tests, `StyioSamples.MatrixOperationsAndIntrinsics`, and runtime C API coverage. |

Matrix intrinsics remain compiler-owned because the compiler must track element kind, static shape,
runtime helper selection, and fail-closed diagnostics before codegen. They are not `std.math` or
`std.collections` APIs until a standard-library promotion record updates `library/manifest.json`
and the catalog.

### IO Helper Intrinsics

`src/StyioUtil/IOIntrinsics.hpp` selects runtime helper names for typed stdin list pulls. The
active element families are i64/int aliases, f64/float aliases, and string aliases. This is a
compiler/runtime helper route for accepted stdin syntax, not a public `std.io` API.

## Deferred Candidates

The following surfaces are design candidates only in this document:

| Candidate | Required before acceptance |
|-----------|----------------------------|
| `[min, n]`, `[std, n]`, `[ema, n]`, `[rsi, n]` | Parser recognition, AST tests, Sema/type rules, state-slot classification, lowering, codegen/runtime behavior, positive execution tests, adjacent unsupported diagnostics, and catalog updates. |
| `cross_over(a, b)`, `cross_under(a, b)` | Accepted source form, history-selector decision, type rules, execution semantics, and negative tests for unavailable history. |
| `is_valid(x)` and `x ?? reason` | Absence metadata contract, source syntax, type behavior, lowering/runtime route, and diagnostics. |
| Value-level `x \| default` absence fallback | A source-level absence value contract distinct from current resource-effect `?\| op \| fallback` behavior. |
| Order dispatch and global state snapshot examples | Resource driver or runtime contract, trust/failure model, and tests. |
| Vectorization or `alwaysinline` guarantees | Codegen-shape tests or benchmark evidence tied to the exact intrinsic path. |

Deferred candidates must not be cited as active language, standard-library, or performance behavior.

## Admission Checklist

Before adding or promoting an intrinsic:

1. Update this document with the exact accepted surface and explicit non-goals.
2. Add Sema/type rules and fail-closed diagnostics for adjacent unsupported forms.
3. Add lowering and codegen/runtime evidence.
4. Add positive execution coverage and negative diagnostics.
5. Update `workflows/TEST-CATALOG.md` with the maintaining gate.
6. Update IM-D8 when the capability changes the compiler-intrinsic vs standard-library boundary.
7. Update `library/manifest.json` only when the capability becomes an active standard-library module or API.
