# External benchmark integration

Performance workloads, runners, probes, reports, and benchmark-specific tests
are owned by `styio-benchmark`. This directory contains only the optional CMake
integration seam used to compile benchmark probes against this Styio checkout.

Standalone Styio builds register no benchmark targets. To enable the external
integration explicitly:

```bash
cmake -S . -B build/benchmark \
  -DSTYIO_BENCHMARK_ROOT=/path/to/styio-benchmark \
  -DSTYIO_REQUIRE_EXTERNAL_BENCHMARK=ON
cmake --build build/benchmark --target \
  styio_core_bench styio_soak_test styio_task_scheduler_perf_test
```

There is no sibling, home-directory, environment, or fallback discovery. A
required but incomplete root fails configuration with the missing asset list.

## Observable static snapshot profiler handoff

The full compiler may record one opt-in frontend phase named
`observable_static_snapshot` when a compile plan requests schema-v1 snapshot
emission. Counters are diagnostic only and are not local thresholds:

- `snapshot_node_count`
- `snapshot_edge_count`
- `snapshot_fact_count`
- `snapshot_anchor_count`
- `snapshot_evidence_count`
- `snapshot_serialized_bytes`

Reproduce without enabling the external benchmark repository:

```bash
styio --profile-frontend --profile-out snapshot-profile.json --compile-plan path/to/plan.json
```

The external `styio-benchmark` owner decides whether to collect these fields,
which workloads to run, and whether any threshold belongs in that repository.
This tree must not create `build/benchmark` or edit the external benchmark
checkout for this incubating stage.
