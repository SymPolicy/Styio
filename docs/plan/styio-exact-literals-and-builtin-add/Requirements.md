# Q05-LIT-ADD Requirements

**Purpose:** Define the bounded product and compiler obligations for the accepted exact-literal materialization and closed scalar `Add` contract.

**Last updated:** 2026-07-20

**Plan ID:** `a0063a94-8e76-4121-937a-0a43fa94b8d1`

## Authority and users

[Styio-Exact-Literals-and-Builtin-Add.md](../../design/Styio-Exact-Literals-and-Builtin-Add.md) is the sole semantic owner. This file translates that accepted contract into compiler, tooling, performance, migration, and evidence obligations without adding source forms or future operator families.

Primary users are authors writing numeric literals and scalar `+`, library authors relying on stable completion facts, and IDE/compiler integrators who require the same type and diagnostic result regardless of host, optimization level, call order, or incremental-cache state.

## Functional requirements

### REQ-LITADD-001 — Exact source values

Before materialization, an integer literal is an arbitrary-precision signed mathematical integer. A decimal literal is an exact sign, non-negative arbitrary-precision coefficient, and checked base-ten exponent, plus an explicit-negative-zero fact. Canonicalization makes integer `-0` equal to `0`, removes redundant decimal coefficient zeros without changing value, and preserves decimal `-0.0`. The representation is semantic compiler state, not a runtime or author-visible type. Existing grammar remains unchanged.

### REQ-LITADD-002 — Deterministic resource budgets

Numeric processing has finite, versioned `max_numeric_token_bytes`, `max_significant_digits`, `max_exact_integer_bits`, `max_abs_decimal_exponent`, and `max_literal_work_units` limits. Every counter uses checked/saturating arithmetic; lexing rejects oversized tokens before arbitrary-precision allocation, and exact arithmetic precharges size-dependent work before expansion. Limits and diagnostics are independent of wall-clock time, host word size, allocation luck, target, optimization level, and source order. Exceeding a limit reports a compiler-resource error without truncation, early rounding, wrapping, type switching, or `overflow` completion.

### REQ-LITADD-003 — Closed materialization and late defaults

Materialization implements only the owner table: exact integers fit signed widths or convert to `f32`/`f64` only when exact; exact decimals never implicitly convert to integer and convert to floats with round-to-nearest-ties-to-even unless a finite source becomes infinity. Subnormal results and signed decimal zero are preserved. Expected/context types apply first. Still-unconstrained ordinary-value boundaries default integer to `i64` and decimal to `f64` exactly once; out-of-default-range values require explicit admitted types and never cause magnitude-based widening. No default occurs inside a generalizable callable scheme.

### REQ-LITADD-004 — One closed scalar `Add` relation

One compiler-owned finite catalog admits only `i8`, `i16`, `i32`, `i64`, `i128`, `f32`, and `f64`. It contains identical concrete `T + T -> T` rows and symmetric `T + ExactLiteral -> T` rows when materialization succeeds. Concrete `i32+i64`, `i64+f64`, `f32+f64`, and every `bool`, `char`, `byte`, string, container, or matrix scalar-`Add` candidate reject. There is no common-type promotion, open overload search, backend-selected result, hidden conversion, or hidden completion row.

### REQ-LITADD-005 — Q02 principal-inference seam

Q05 publishes immutable relation queries, stable row identities/catalog fingerprinting, exact-literal constraints, and a finite conservative completion upper bound. Q02 alone owns type variables, solving, generalization, instantiation, and monomorphization. A generalized scalar `Add` constraint unions legal row completions to `{overflow}`; `add_five` is not defaulted at definition time, and a later `f64` instance does not shrink that scheme-level obligation. No duplicate relation catalog or solver is introduced in either plan.

### REQ-LITADD-006 — Checked signed-integer completion

Every selected signed-integer row performs width-correct checked addition. An upper or lower range violation follows the existing direct completion CFG with the single resolved, payload-free `overflow` family; it never wraps, invokes LLVM undefined behavior, traps outside the completion model, or varies by build mode. Statically known overflow produces the same edge. The operation summary is `{success_type=T, completion_set={overflow}}` and settlement/propagation remains owned by existing completion infrastructure.

### REQ-LITADD-007 — Strict IEEE floating behavior

Every selected `f32`/`f64` row uses round-to-nearest-ties-to-even, gradual underflow, subnormal and signed-zero preservation, and no host rounding-mode dependence. Infinity and NaN are permitted results and do not produce `overflow`. Generated code and optimizers may not introduce fast-math flags, reassociation, FTZ/DAZ, `nsz`, `nnan`, or `ninf` assumptions. Each supported target proves the strict contract; an unsupported target fails closed or uses a repository-owned deterministic helper whose bit behavior is verified.

### REQ-LITADD-008 — Const/runtime and typed-IR identity

Exact simplification may remain provisional, but after a row is selected, constant and runtime evaluation consume the same materialization rule and operation descriptor. SGIR carries concrete width/format, row identity, and finite completion summary; its verifier rejects unresolved/`Undefined` numeric types, inconsistent operands/results, missing integer completion edges, and floating rows with `overflow`. Constant evaluation cannot bypass materialization failure, rounding, or completion.

### REQ-LITADD-009 — Diagnostics, IDE, and cache determinism

Stable source-located diagnostics distinguish lexical/resource exhaustion, invalid exact spelling, materialization failure, default-range failure, concrete-type mismatch, excluded/unsupported `Add`, unhandled `overflow`, and unsupported strict-FP target. CLI and IDE expose the same classification and selected concrete type. Semantic/cache keys use canonical literal encoding, relation-catalog fingerprint, versioned limit configuration, and strict-FP target contract rather than host values, raw pointers, unordered iteration, or session order.

### REQ-LITADD-010 — Complete migration and bounded exclusions

The final route deletes current `getMaxType` promotion for `Add`, numeric/string coercion and concatenation through scalar `+`, host `stol`/`stoll`/`stod`/`to_string` literal evaluation, pointer-to-number backend coercion, width-erasing integer/float mappings, unchecked language integer add, permissive mixed-float add, and executable `Undefined`/`i64` repair. Obsolete tests and documents are deleted or rewritten in the same change. The migration does not delete independently owned non-`Add` matrix/container capabilities or decide deferred syntax and relations.

### REQ-LITADD-011 — Verification and non-functional envelope

The implementation provides boundary, property/differential, const/runtime, optimization, target, diagnostic, IDE/cache, and end-to-end tests for every admitted type and excluded family. Fixed-corpus benchmarks record parse/materialization/fold throughput, peak memory, catalog lookup cost, and incremental-cache behavior. Finite default thresholds are selected from checked-in evidence before enablement, tested at the limit and one beyond, and guarded against asymptotic or allocation regressions. Final evidence is reproducible on one revision and includes structural old-path searches.

## Constraints and invariants

- Exact values have one owner and are not duplicated as strings, host numbers, and semantic numbers with competing authority.
- Parsing and canonicalization are linear in accepted token length; catalog lookup is bounded by the finite scalar domain; arbitrary-precision operations are charged from operand sizes before work begins.
- No elapsed-time or memory-allocation exception is a semantic budget decision.
- No unresolved exact literal, type variable, or generic numeric family reaches executable SGIR or LLVM.
- Catalog and limit configuration are immutable during one compilation session and safe for concurrent read access.
- A cache hit cannot weaken validation, materialization, completion, or strict-FP target checks.

## Explicit non-goals

- Radix/separator/exponent grammar changes, unsigned or platform-width integers, additional floats, decimal runtime types, and explicit conversion syntax.
- Text concatenation, collection/matrix arithmetic, other arithmetic operations, NaN equality/order/payload policy, or wrapping/saturating modes.
- Q02 inference algorithms, accepted Q03-F evaluation/effect ordering and its `EvaluationFacts`/DAG/CFG implementation, generic completion settlement, or F02 author-defined operators.

## Final acceptance target

On one source revision, the checked-in configuration, catalog fingerprint, compiler/LLVM target facts, targeted/full tests, optimization/IR audits, deterministic replay, performance/memory receipts, documentation checks, and structural-removal searches demonstrate all eleven requirements. No compatibility route or obsolete test expectation remains.
