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
