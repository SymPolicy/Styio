# Q05-LIT-ADD Validation

**Purpose:** Map every Q05-LIT-ADD requirement to executable, structural, differential, resource, tooling, migration, and documentation evidence.

**Last updated:** 2026-07-20

**Plan ID:** `a0063a94-8e76-4121-937a-0a43fa94b8d1`

## Evidence rule

Every result is captured on one revision with compiler version, LLVM version, target triple/features, build mode, limit-config fingerprint, catalog fingerprint, command, exit code, and relevant artifact digest. All checkpoints begin unchecked; this matrix is a design contract and does not assert that tests already pass.

## Requirement matrix

| Requirement | Executable evidence | Static/structural evidence |
|---|---|---|
| `REQ-LITADD-001` | Exact integer/decimal parse and canonicalization unit tests; equivalent spellings; integer `-0`; decimal `-0.0`; randomized round-trip against a mathematical reference model. | AST stores spelling/location only; semantic side table has one `ExactLiteral` authority; no new grammar production. |
| `REQ-LITADD-002` | Each token/digit/bit/exponent/work gate at `N-1`, `N`, `N+1`; saturated counter cases; adversarial decimal-alignment/fold inputs; cold/warm-cache replay. | Preflight precedes AP allocation; fixed versioned config; no clock/RSS/allocation-failure semantic branch. |
| `REQ-LITADD-003` | Signed min/max and one-beyond for every width; integer-to-float exactness around `2^24`/`2^53`; decimal ties-even, subnormal, signed zero, finite-to-infinity; decimal-to-int rejection; contextual/default/generalization cases. | Only enumerated concrete-boundary API defaults; no magnitude widening or generalization default call. |
| `REQ-LITADD-004` | Table-driven full row matrix, both literal positions, all concrete mismatches, and bool/char/byte/string/container/matrix exclusions. | Catalog dump/fingerprint matches the canonical seven-type row set; no fallback/common-type row. |
| `REQ-LITADD-005` | Q02 `identity` regression plus `add_five` scheme/independent `i64` and `f64` instances; scheme completion remains `{overflow}`; definition/call order and cache replay are stable. | Q02 owns type/scheme/instance data; Q05 exports only query/bound/fingerprint interface; no duplicate catalog. |
| `REQ-LITADD-006` | Exhaustive `i8` operand pairs; boundary and seeded differential cases for wider widths; handled/propagated/unhandled completion; compile-time-known and runtime overflow equivalence. | LLVM contains `sadd.with.overflow.iN` and direct successors, no language `nsw`/`nuw`/naked add; `overflow` has no payload. |
| `REQ-LITADD-007` | `f32`/`f64` bit-pattern corpus for ties, signed zero, cancellation, normal/subnormal boundaries, infinity, and NaN class at O0/O2/O3/LTO; hostile host rounding mode where supported. | Constrained add, explicit rounding, strict/denormal attributes, supported-target receipt, and no fast-math/FTZ/DAZ/reassociation flags. |
| `REQ-LITADD-008` | Differential const/runtime execution for every row and failure/completion boundary; SGIR negative-verifier fixtures. | Concrete widths/formats/row IDs/completion sets only; no unresolved or `Undefined` executable numeric state. |
| `REQ-LITADD-009` | CLI/LSP category, range, hover, cold/warm cache, collision, invalidation, parallel-session, and source-order equality tests. | Cache schema contains canonical literal/catalog/limits/target facts and excludes raw pointers/session-order keys. |
| `REQ-LITADD-010` | Migrated regression/security/feature fixtures; owner-aware matrix/string inventory; relevant full suite. | Structural searches prove old scalar paths and obsolete fixtures absent without deleting separately owned operations. |
| `REQ-LITADD-011` | Fixed-corpus parse/materialize/fold/catalog/cache benchmarks and peak-memory runs; deterministic repeated builds; targeted plus full test gates. | Threshold selection receipt, complexity review, build/source registration, docs audit, Better Plan validation. |

## Focused test suites

Implementation registers focused CTest labels/targets rather than extending only monolithic fixtures:

- `numeric_literal_exact` — exact representation, canonical serialization, source ranges, negative zero, fuzz/property corpus.
- `numeric_literal_limits` — all five gates, saturated arithmetic, allocation preflight, deterministic work formulas.
- `numeric_materialization` — every target/range, exact integer-to-float, decimal rounding/underflow/infinity, late defaults.
- `builtin_add_relation` — complete catalog, symmetric literal rows, excluded families, Q02 narrow seam and fingerprint.
- `builtin_add_const_runtime` — APInt/APFloat evaluator, checked completion, literal-folding boundary, SGIR verifier.
- `builtin_add_codegen` — integer intrinsic/control edges, concrete type mapping, constrained floating IR, optimization/target matrix.
- `numeric_diagnostics_ide` — classifications, CLI/LSP parity, hover, incremental invalidation, collision and concurrency.
- `numeric_semantics_benchmark` — fixed corpus, operation counts, throughput, peak memory, and cache behavior.

The final node runs the repository's normal configure/build command, `ctest --test-dir <build> --output-on-failure -L numeric`, the relevant existing Sema/lowering/codegen/security/IDE labels, and the repository full test gate. The recorded receipt substitutes the actual repository build directory for `<build>`; it does not hard-code a machine path.

## Boundary and oracle details

### Exact values and budgets

- Test empty/invalid forms only to the extent the existing grammar can produce them; no test accidentally admits new radix/separator/exponent spelling.
- Compare canonical integers and coefficient/exponent pairs against an independent arbitrary-precision reference in tests, including redundant zeros and signs.
- Generate exact cases at each configured digit/bit/exponent boundary. Assert `N` is processed and `N+1` yields the precise resource category before large allocation.
- Charge decimal exponent alignment and exact literal-literal operations using inputs whose metadata is small but expansion would be large. Assert work rejection is deterministic across debug/release and repeated runs.

### Materialization and defaults

- For `i8/i16/i32/i64/i128`, test signed minimum, maximum, zero, and one outside each side.
- For integer-to-float, cover `2^24`, `2^24+1`, `2^53`, `2^53+1`, negatives, and larger exactly representable powers.
- Build decimal halfway/tie cases whose adjacent IEEE values have even/odd low significand bits; compare exact result bits with an independent APFloat/reference fixture.
- Cover smallest normal, largest subnormal, smallest subnormal, underflow to signed zero, explicit `-0.0`, maximum finite, and finite-to-infinity rejection for both formats.
- Exercise explicit expected types, literal plus concrete context, ordinary unannotated storage, non-generic return, generalized callable definition, and out-of-default-range diagnostics. Assert the compiler never auto-selects `i128`.

### Relation and inference

- Generate the catalog oracle directly from the seven-type owner set, not from production output. Check concrete/concrete identity rows and every admitted/rejected literal shape in both orders.
- Verify `i32+i64`, `i64+f64`, `f32+f64`, string/numeric, matrix/scalar, boolean, character, byte, and container cases fail with no-row/mismatch classifications and no backend code.
- Inspect Q02's canonical scheme for `add_five`: the exact `5` constraint remains symbolic, parameter/result share the admitted scalar, and completion upper bound is `{overflow}`. Calls do not mutate the definition or catalog.

### Checked integer and strict floating execution

- Exhaustively compare all 65,536 `i8` pairs with a wide mathematical oracle, including exact sum or `overflow` edge. Use boundary partitions and fixed-seed random cases for wider widths.
- Compile the same selected constant once folded and once forced runtime; compare success bits or resolved completion family/control destination.
- Audit optimized IR for `sadd.with.overflow` at each integer width and direct conditional successors; reject `nsw`, `nuw`, naked language `add`, implicit casts, or traps outside settlement.
- Compare float result bits at O0/O2/O3/LTO for cancellation, signed zero, subnormals, overflow to infinity, infinity operations, and representative quiet/signaling NaN classes without asserting deferred payload canonicalization.
- Run under non-default host rounding where the test platform supports it; result bits must remain nearest-ties-to-even. Verify the target capability gate on every supported triple and one deliberately unsupported fixture.

## Diagnostics, IDE, cache, and determinism

Golden diagnostics separately cover resource exhaustion, parse/canonicalization failure, materialization, default range, concrete mismatch, excluded family, missing overflow settlement, and unsupported strict-FP target. Each golden checks stable code/category, primary range, bounded notes, and no raw host/backend exception.

CLI and IDE operate on the same source corpus and compare normalized semantic results. Hover at a materialized boundary shows the concrete type and never presents an author-visible arbitrary-precision type. Cold parse, warm cache, edit-invalidated cache, deliberate hash collision, reversed file/call order, and concurrent sessions must produce identical catalog selection and diagnostics. A limits/catalog/target digest change invalidates affected facts.

## Performance and memory evidence

Before production enablement, the implementation checkpoint records concrete finite values for all five limits from a checked-in corpus containing small normal files, boundary literals, large accepted integers/decimals, repeated literals, exponent-alignment stress, constant-fold stress, and hostile rejected inputs.

The receipt records median and tail parse/materialize/fold time, deterministic work units, catalog queries, peak resident memory through the repository's benchmark mechanism, cache size/hit behavior, and build configuration. Acceptance requires:

- linear accepted-token scaling within measurement noise and no superlinear lexer/canonicalizer trend;
- catalog lookup independent of source magnitude and bounded by the finite table;
- peak memory proportional to accepted limb storage and below the evidence-approved threshold;
- hostile `N+1` inputs rejected before large expansion;
- no material regression against the recorded pre-merge baseline unless explicitly justified by correctness evidence.

Elapsed time and sampled RSS are performance evidence only, never semantic admission gates.

## Structural removal gates

The final receipt includes scoped `rg` searches and manual classification proving:

- scalar `Add` no longer calls `getMaxType`, `infer_concat_string_add`, or `infer_numeric_string_coercion`;
- numeric literal/materialization/folding paths no longer use `std::stol`, `std::stoll`, `std::stod`, `std::to_string`, host `long`, or host `double` as semantic authority;
- backend scalar `Add` has no string-pointer numeric coercion, mixed int/float promotion, width-erasing default, naked checked-language integer add, permissive ordinary floating fallback, or forbidden fast-math flags;
- executable SGIR has no `Undefined` numeric repair and constants do not default silently to 64-bit/double;
- tests/docs no longer require removed promotion, string-plus-number, or backend-default behavior;
- matrix/container/text paths that remain are explicitly mapped to a separate owner/operation identity, not silently retained as Q05 rows.

Search terms are evidence starters, not the proof by themselves; reviewers inspect aliases/wrappers and record every remaining match with an owner and reason.

## Final gate

The final validation node passes only when all requirement rows have executable and structural receipts, focused and full suites pass, supported-target strict-FP evidence is complete, budgets and performance baselines are checked in, old routes/tests are absent, docs agree with the owner, `Checkpoints.json` validates, and all evidence refers to the same source revision.
