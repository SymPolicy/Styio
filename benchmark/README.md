# `benchmark/` - Styio Probe And Adapter Surface

The canonical benchmark repository is `styio-benchmark`.

All performance workloads, benchmark runners, cross-runtime harnesses, native C++
comparison code, stored reports, baseline documents, and migrated C++ probe
sources belong there. This directory remains only for interfaces that must live
next to the Styio build:

- `CMakeLists.txt`
  - Builds Styio-owned probe binaries from
    `styio-benchmark/styio-probes/*.cpp`.
- `parser-shadow-suite-gate.sh`
  - Parser correctness gate used by Styio CTest feature suites.
- `perf-route.sh`, `perf-report.py`, and `soak-minimize.sh`
  - Active adapters that locate `styio-benchmark` and forward with `--styio-root`
    set to this checkout.
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
measurement needs Styio internals, put the smallest necessary probe source under
`styio-benchmark/styio-probes/`, expose only the target or ABI needed by this
checkout, then keep the workload and runner in `styio-benchmark`.
