# Benchmark Results — Current Only (de1f341)

**Date:** 2026-06-25
**Commit:** `de1f341`
**Build:** Release

## Evidence Summary

Commit `6e59b68` lacks a `styio_core_bench` target, so baseline comparison is unavailable. Evidence: benchmark binary builds and runs; JSON output at `benchmark/results/current.json`.

## Raw Results (30 iterations, median)

| Benchmark | Median | Phase |
|-----------|--------|-------|
| lex/large_file_10k | 4.23 ms | Tokenizer |
| parse/many_stmts_1k | 10.9 ms | Parser |
| parse/deep_expr_200 | 0.19 ms | Parser |
| sema/name_resolution_5k | 592 ms | Semantic analysis |
| type/typed_bindings_1k | 32.3 ms | Type checking |
| topology/resource_dag_200 | 12.0 ms | Resource topology |
| diag/many_errors_100 | 0.06 ms | Diagnostics |
| runtime/task_spawn_1k_nop | 0.6 µs | Runtime (nop baseline) |

## Infrastructure Established

- [x] Benchmark binary builds in Release mode without crash
- [x] JSON output with commit hash, build type, timestamps
- [x] 8 benchmark scenarios covering lex/parse/sema/type/topology/diag/runtime
- [x] Route cache counters available via `STYIO_PARSER_ROUTE_CACHE_STATS=1`
- [x] IR allocation stats struct available for future instrumentation
- [x] Route cache counters now emitted in route-cache report (`scripts/benchmark-compare.py --route-cache`)

## Next Steps for Performance Proof

1. Build benchmark on `6e59b68` (or build `styio` binary and use `--styio-bin` mode)
2. Compare current vs baseline with `scripts/benchmark-compare.py --threshold 5 --markdown`
3. Keep route cache counter evidence current (`benchmark/results/current-route-cache-report.md`)
4. Add IR allocation counter readout to benchmark JSON
