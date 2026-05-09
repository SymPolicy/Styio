# `benchmark/` - Styio Probe And Adapter Surface

The canonical benchmark repository is `styio-benchmark`.

All performance workloads, benchmark runners, cross-runtime harnesses, native C++
comparison code, stored reports, and baseline documents belong there. This
directory remains only for interfaces that must live next to the Styio build:

- `CMakeLists.txt`
  - Builds Styio-owned probe binaries.
- `styio_soak_test.cpp`
  - In-tree long-loop and phase probe binary consumed by external routes.
- `styio_task_scheduler_perf_test.cpp`
  - In-tree task scheduler probe binary consumed by external async routes.
- `parser-shadow-suite-gate.sh` and `parser-shadow-m1-gate.sh`
  - Parser correctness gate wrappers used by Styio CTest.
- `perf-route.sh`, `perf-report.py`, and `soak-minimize.sh`
  - Compatibility adapters that locate `styio-benchmark` and forward with
    `--styio-root` set to this checkout.
- `regressions/` and `reports/`
  - Local-only output locations. Do not commit generated benchmark artifacts
    here; promote durable reports in `styio-benchmark` instead.

Use the external benchmark route directly when possible:

```bash
STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark \
  ./benchmark/perf-route.sh --quick
```

Equivalent direct form:

```bash
/path/to/styio-benchmark/tools/perf-route.sh --styio-root /path/to/styio --quick
```

Do not add new benchmark implementation files under this directory. If a new
measurement needs Styio internals, add the smallest necessary exported probe or
ABI here, then keep the workload and runner in `styio-benchmark`.
