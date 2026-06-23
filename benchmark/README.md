# `benchmark/` - Styio Benchmark Boundary

The canonical benchmark repository is `styio-benchmark`.

Deep performance workloads, cross-runtime harnesses, native C++ comparison code,
stored reports, baseline documents, and migrated C++ probe sources belong there.
This repository still keeps a tiny core corpus so a Styio checkout can always
produce local, machine-readable performance evidence without an external clone.

- `CMakeLists.txt`
  - Registers the in-repo core benchmark smoke and, when available, builds
    Styio-owned probe binaries from `styio-benchmark/styio-probes/*.cpp`.
- `core/manifest.json` and `core/run-core.py`
  - Minimal reproducible workloads backed by existing algorithm fixtures. These
    prove executable coverage and timing-schema stability; they are not
    cross-runtime comparison evidence.
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

Do not add broad benchmark suites under this directory. If a new measurement
needs Styio internals or cross-runtime comparison, put the smallest necessary
probe source under `styio-benchmark/styio-probes/`, expose only the target or ABI
needed by this checkout, then keep the workload and runner in `styio-benchmark`.
Only add to `benchmark/core/` when the workload is small, deterministic, already
has behavior coverage in this repository, and is useful as release-conformance
evidence.
