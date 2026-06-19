# Benchmark Coverage Boundary

The benchmark coverage matrix is maintained in `styio-benchmark`:

```text
styio-benchmark/docs/COVERAGE-MATRIX.md
```

Styio keeps only build-local probe adapters and active handoff adapters under
`benchmark/`; migrated probe sources live in `styio-benchmark/styio-probes/`.
Coverage changes, route changes, comparison workloads, historical baselines, and
generated reports must be made in `styio-benchmark`.

Current Styio-owned probe surface:

- `styio_soak_test`
- `styio_task_scheduler_perf_test`
- parser shadow gate scripts used by CTest
- runtime/compiler ABI needed by external benchmark harnesses
