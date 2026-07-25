# Styio Compiler Intrinsics and Standard-Library Boundary

**Purpose:** Record compiler-owned intrinsic surfaces with implementation evidence, and keep deferred algorithm ideas separate from standard-library promises.

**Last updated:** 2026-07-20

---

## Scope

This document governs capabilities that need compiler ownership for parsing, type inference,
lowering, runtime helper routing, pulse-state storage, or code generation.

Compiler intrinsics are not automatically standard-library APIs. A capability may be active as a
compiler intrinsic while `library/manifest.json` still records only `std.resource` as an active
standard-library compatibility module. Planned standard-library modules remain reservations until
they have source, tests, diagnostics, and manifest evidence.

### Closed semantic relations and prelude identities

Accepted `Q05-LIT-ADD` is owned by
[Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md).
Its finite scalar `Add` table is compiler-owned but is not a callable intrinsic,
public trait, overload set, or standard-library API: authors continue to write
the existing `+` operator. Constant evaluation, Sema, lowering, and runtime
helpers must implement the same closed rows and may not expand them from helper
availability.

The prelude supplies one nominal payload-free completion-family identity named
`overflow` for checked signed-integer `Add`. It is not a function, intrinsic,
keyword, token, exception class, or manifest-promoted module API. The ordinary
identifier is resolved in completion contracts and settlement arms; its
semantic existence does not authorize editing `library/manifest.json` or
treating unrelated prelude APIs as delivered.

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

The accepted source surface for series intrinsics is ordinary call syntax —
`avg(series, n)` / `max(series, n)` — recognized as compiler intrinsics during
semantic analysis, the same model as the matrix helpers below. The word-mode
selector spelling `series[avg, n]` is removed from the design; the current
parser still recognizes it inside brackets, and that path is compatibility
debt that must converge on a migration diagnostic.

| Surface | Current status | Evidence | Limits |
|---------|----------------|----------|--------|
| `avg(series, n)` | Active compiler intrinsic for the pulse/state path; current implementation still enters through the removed bracket spelling `series[avg, n]` (compatibility debt) | Parser recognizes `avg` in `src/StyioParser/Parser.cpp`; AST is `SeriesIntrinsicAST`; lowering emits `SGSeriesAvgStep` in `src/StyioLowering/AstToStyioIR.cpp`; codegen is in `src/StyioCodeGen/CodeGenPulse.cpp`; parser, lowering, ownership, representation, and codegen tests live in `tests/security/styio_security_test.cpp`. | `n` must be an integer literal. Lowering requires an enclosing state slot. The current i64 pulse-ledger sentinel used for cold start or skipped input is non-conforming implementation debt: the public result must become `? \| i64` with `(?)`, or the path must fail closed until migrated; an ordinary `i64` may never expose the sentinel. |
| `max(series, n)` | Active compiler intrinsic for the pulse/state path; current implementation still enters through the removed bracket spelling `series[max, n]` (compatibility debt) | Parser recognizes `max`; lowering emits `SGSeriesMaxStep`; codegen is in `src/StyioCodeGen/CodeGenPulse.cpp`; focused tests cover parser AST, state-slot classification, IR lowering, and codegen shape. | Same pulse/state limits as `avg`. The current codegen scans the bounded ring for the current window; do not document a monotonic-deque guarantee without new evidence. |

Deferred series operators: `min(series, n)`, `std(series, n)`,
`ema(series, n)`, `rsi(series, n)`, `cross_over`, and `cross_under` are not
active compiler contracts in this checkout. They require the full evidence
list above before being described as accepted behavior, and they land as
ordinary calls only — no word-mode selector spelling will be added.

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

These implementation surfaces do not add matrices to the closed scalar `Add`
table, freeze mixed-kind matrix coercion, or prove a general matrix `+`/`-`/`*`
language relation. Those semantics remain deferred to their later `Q05`/`Q08`
owner decision even where named helpers currently execute.

### IO Helper Intrinsics

`src/StyioUtil/IOIntrinsics.hpp` selects runtime helper names for typed stdin list pulls. The
active element families are i64/int aliases, f64/float aliases, and string aliases. This is a
compiler/runtime helper route for accepted stdin syntax, not a public `std.io` API.

## Deferred Candidates

The following surfaces are design candidates only in this document:

| Candidate | Required before acceptance |
|-----------|----------------------------|
| `min(series, n)`, `std(series, n)`, `ema(series, n)`, `rsi(series, n)` | Sema intrinsic recognition of the ordinary call form (no word-mode selector spelling), AST tests, Sema/type rules, state-slot classification, lowering, codegen/runtime behavior, positive execution tests, adjacent unsupported diagnostics, and catalog updates. |
| Stride selector `x[%n]` | Design-accepted selection surface (keep index ≡ 0 mod n; `[%1]` identity; `[%0]` rejected). Requires parser recognition, Sema rules, lowering, codegen/runtime behavior, and tests; fails closed until then. |
| `cross_over(a, b)`, `cross_under(a, b)` | Accepted source form, history-selector decision, type rules, execution semantics, and negative tests for unavailable history. |
| `is_valid(x)` | Optional/absence predicate contract, type behavior, lowering/runtime route, and diagnostics; this candidate assigns no role to `??`. |
| Order dispatch and global state snapshot examples | Resource driver or runtime contract, trust/failure model, and tests. |
| Vectorization or `alwaysinline` guarantees | Codegen-shape tests or benchmark evidence tied to the exact intrinsic path. |

Deferred candidates must not be cited as active language, standard-library, or performance behavior.
Optional value fallback is not a deferred candidate: D02 is closed with no
ordinary value-level fallback/coalescing operator. Neither bare `x | default`
nor `x ?? default` may be admitted, and no standard-library intrinsic may
simulate either through type or purity inference. The grammar-anchored
`?| operation | fallback` resource/effect settlement form remains separate.

## Admission Checklist

Before adding or promoting an intrinsic:

1. Update this document with the exact accepted surface and explicit non-goals.
2. Add Sema/type rules and fail-closed diagnostics for adjacent unsupported forms.
3. Add lowering and codegen/runtime evidence.
4. Add positive execution coverage and negative diagnostics.
5. Update `workflows/TEST-CATALOG.md` with the maintaining gate.
6. Update IM-D8 when the capability changes the compiler-intrinsic vs standard-library boundary.
7. Update `library/manifest.json` only when the capability becomes an active standard-library module or API.
