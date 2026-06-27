| Benchmark | Baseline | Current | Delta % | Status |
|-----------|----------|---------|---------|--------|
| diag/many_errors_100 | 80.54 us | 80.54 us | +0.0% | stable |
| lex/large_file_10k | 4.23 ms | 4.23 ms | +0.0% | stable |
| parse/deep_expr_200 | 194.08 us | 194.08 us | +0.0% | stable |
| parse/many_stmts_1k | 10.92 ms | 10.92 ms | +0.0% | stable |
| route_cache/deep_expr_200 | 0 ns | 0 ns | +0.0% | stable |
| route_cache/many_stmts_1k | 0 ns | 0 ns | +0.0% | stable |
| route_cache/name_resolution_5k | 0 ns | 0 ns | +0.0% | stable |
| route_cache/resource_dag_200 | 0 ns | 0 ns | +0.0% | stable |
| route_cache/typed_bindings_1k | 0 ns | 0 ns | +0.0% | stable |
| runtime/task_spawn_1k_nop | 2.54 us | 2.54 us | +0.0% | stable |
| sema/name_resolution_5k | 1.32 s | 1.32 s | +0.0% | stable |
| topology/resource_dag_200 | 22.22 ms | 22.22 ms | +0.0% | stable |
| type/typed_bindings_1k | 57.72 ms | 57.72 ms | +0.0% | stable |

**Summary:** 0 regression(s), 0 improvement(s), 0 new, 0 missing
| Benchmark | Metric | Baseline | Current | Delta |
|----------|--------|----------|---------|-------|
| route_cache/deep_expr_200 | scan | 1 | 1 | +0 |
| route_cache/deep_expr_200 | miss | 1 | 1 | +0 |
| route_cache/deep_expr_200 | hit | 0 | 0 | +0 |
| route_cache/deep_expr_200 | disabled | 0 | 0 | +0 |
| route_cache/many_stmts_1k | scan | 1000 | 1000 | +0 |
| route_cache/many_stmts_1k | miss | 1000 | 1000 | +0 |
| route_cache/many_stmts_1k | hit | 0 | 0 | +0 |
| route_cache/many_stmts_1k | disabled | 0 | 0 | +0 |
| route_cache/name_resolution_5k | scan | 5001 | 5001 | +0 |
| route_cache/name_resolution_5k | miss | 5001 | 5001 | +0 |
| route_cache/name_resolution_5k | hit | 0 | 0 | +0 |
| route_cache/name_resolution_5k | disabled | 0 | 0 | +0 |
| route_cache/resource_dag_200 | scan | 600 | 600 | +0 |
| route_cache/resource_dag_200 | miss | 600 | 600 | +0 |
| route_cache/resource_dag_200 | hit | 0 | 0 | +0 |
| route_cache/resource_dag_200 | disabled | 0 | 0 | +0 |
| route_cache/typed_bindings_1k | scan | 1200 | 1200 | +0 |
| route_cache/typed_bindings_1k | miss | 1200 | 1200 | +0 |
| route_cache/typed_bindings_1k | hit | 0 | 0 | +0 |
| route_cache/typed_bindings_1k | disabled | 0 | 0 | +0 |
