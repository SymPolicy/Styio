# Parser Core Unification Validation

**Purpose:** Freeze executable acceptance for OPT-C before production implementation.

**Last updated:** 2026-08-02

**Status:** Frozen. Commands run only at their Better Plan lifecycle boundary.

## Evidence rules

1. The Designer owns the direct tests and benchmark probes. Production Workers must make them pass without weakening inputs, bounds, exception classes, messages, counters, or AST assertions.
2. Each implementation Node receives one focused regression after its Verifier. No full suite runs between Nodes.
3. After both implementations complete, one fresh group Reviewer runs once. Only then does the group run the full impacted regression once; a rerun requires concrete failure evidence.
4. Timing values are observations, not hard pass/fail thresholds. Structural counters, parse success, resource limits, zero fallback/bridges, and benchmark sample presence are deterministic gates.

## Frozen direct acceptance matrix

`tests/parser_internal_test.cpp` owns the following `StyioParserInternal` seams:

| Seam | Inputs | Required observation |
|---|---|---|
| metadata authority | every table row plus unsupported bitwise/assignment/resource tokens | exact precedence/association/kind; unsupported lookup is null; power alone is right-associative |
| arithmetic precedence | `1 + 2 * 3`, `1 * 2 + 3`, `8 / 4 % 3` | root/RHS shapes prove multiplicative-before-additive and equal-precedence left association |
| power/prefix | `2 ** 3 ** 2`, `-2 ** 3`, `-name * 2` | right-nested power, signed-numeric atom binding, and preserved non-literal unary-minus shape |
| logic/comparison/fallback | `a == b && c != d || e`, `a | b | c`, `f <| x | y` | existing `CondAST`, `FallbackAST`, and apply/fallback ownership |
| tight postfix | `f(x)(y)[0]`, `xs[0..2]`, line-break-separated `fn\n(1)` / `xs\n[0]` | left-to-right call/index/slice; no postfix crosses a line break |
| extended/resource suffix | `"a" + "b" -> @stdout`, `"a" + "b" >> @stdout` | redirect/write owns the complete additive AST; no reassociation helper |
| allowed follow | delimiter-bounded `1 + 2 | fallback` | parsed `1 + 2`, cursor left on `|`, no fallback consumption |
| fatal ownership | recognized starts with missing RHS, missing member, or unmatched delimiter | existing exception class/message; `try_` result is `Fatal`, saved cursor restored, never `Declined` |
| decline ownership | input not in expression FIRST | `Declined` and byte/token cursor unchanged |
| linear flat chain | at least 4096 additive operands | parse succeeds; `expression_token_visits <= 8 * token_count + 8`; zero scratch allocations; linear AST-node count; shallow expression depth |
| bounded right chain | power chain below and above `kStyioExprMaxDepth` | below-bound success; 129th active frame fails with the frozen resource-limit diagnostic |
| nightly authority | representative scalar/call/index/resource-suffix program | `legacy_fallback_statements == 0` and `nightly_internal_legacy_bridges == 0` |

Malformed-input assertions compare the stable message payload and exception family. Source-line decoration may be asserted separately but may not replace the payload check.

## Benchmark seams

`benchmark/internal/core_bench.cpp` keeps the existing parse corpus and adds:

1. `expr_flat_add_4096`: a long left-associative chain that must parse successfully with linear counters, zero expression-core scratch allocations, and bounded depth.
2. `expr_mixed_4096`: an equal-sized additive/multiplicative chain that exercises repeated precedence changes without rescans.
3. `expr_right_power_64`: a right-associative power chain safely below the recursion bound.

Each sample is emitted under phase `parse_expr`; `alloc_count` records `expression_ast_nodes` as the deterministic AST-allocation proxy. The benchmark process fails if parsing returns null, a zero-fallback/zero-bridge invariant fails, a structural counter exceeds its bound, or scratch allocations are nonzero. Median durations remain JSON evidence for comparison and are not used as a noisy local threshold.

## Focused implementation regression

Run once after the `OPT-C-IMPLEMENT` Verifier:

```bash
cmake --build build --target styio_parser_internal_test styio_core_bench -j2 && ctest --test-dir build -R '^StyioParserInternal\.' --output-on-failure
```

Required result: exit zero and every direct matrix row above is exercised. The benchmark binary is built here; its smoke execution is reserved for the group regression to avoid duplicate timing runs.

## Focused documentation regression

Run once after the `OPT-C-DOCS` Verifier:

```bash
bash scripts/docs-gate.sh
```

Required result: exit zero. Active docs must name one constexpr operator authority and one parser core, state the O(n)/depth/diagnostic/no-fallback contract, and remove wording that presents ordinal precedence or the nightly private switch as a maintained alternative.

## One-time migration audit

During the group Reviewer, run this read-only audit once. It is not added as a permanent test or gate:

```bash
rg -n 'TokenPrecedenceMap|parse_arithmetic_tail_from_atom|parse_relational_expr|parse_and_expr|parse_or_expr|parse_fallback_expr|reassociate_add_into_resource_sink_latest_draft|expr_prec_of|expr_is_right_assoc|expr_map_binop|expr_is_comp|expr_map_comp|expr_is_logic|expr_map_logic' src/StyioParser/Parser.cpp src/StyioParser/Parser.hpp src/StyioParser/NewParserExpr.cpp src/StyioToken/Token.hpp tests/parser_internal_test.cpp
```

Required result: no matches. Separately inspect the common loop and confirm it contains no relational comparison between `StyioOpType` values and no unordered-map lookup.

## Final group validation

After the one group Reviewer reports no unresolved issue, run exactly once:

```bash
cmake --build build -j2 && ctest --test-dir build -L styio_pipeline --output-on-failure && ctest --test-dir build -R 'parser_shadow_gate|zero_fallback|zero_internal_bridges|StyioParserInternal|StyioAlgorithmEquivalence|styio_core_benchmark_smoke' --output-on-failure && bash scripts/docs-gate.sh
```

The group passes only when:

1. accepted scalar, function, resource, iterator, match, call/index/slice, format-string, and resource-method expression fixtures preserve their approved AST/behavior envelope;
2. frozen malformed fixtures preserve exception family, diagnostic payload, and recovery boundary;
3. all authoritative nightly artifacts report zero legacy fallback and zero internal legacy bridges;
4. the old-symbol migration audit is empty and the owner docs describe only the delivered authority;
5. the three `parse_expr` benchmark samples execute and emit valid structured results with their deterministic counter/allocation invariants; and
6. no OPT-D/E, Sema, lowering, runtime, unrelated parser feature, or public syntax surface changed.
