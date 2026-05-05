# Async Runtime Benchmark Framework

This directory contains the cross-runtime async benchmark route for the Styio task scheduler.

The route compares the same two workloads across:

- `styio_task_scheduler`: the repository runtime target.
- `cpp_stackless_coroutine`: a C++20 stackless coroutine scheduler built with coroutine frames and `co_await`.
- `goroutine`: Go goroutines with `GOMAXPROCS` pinned to the worker count.
- `tokio_multi_thread`: Rust Tokio's multi-thread runtime.

## Workloads

1. `sleep`: four `160ms` blocked tasks. This proves whether a runtime overlaps blocked work. A speedup near the worker count means the implementation is truly scheduled rather than eager.
2. `noop`: one hundred thousand trivial tasks. This exposes submit, wake, wait, and release overhead while reducing single-run timer noise.

## Run

```bash
benchmark/async-runtime/run-async-bench.py \
  --bootstrap-toolchains \
  --repeats 5 \
  --out-dir benchmark/async-runtime/reports/<run-id>
```

The script uses `build/async-runtime-release` by default and configures it as a CMake `Release` build when needed. It refuses to use a non-Release Styio build directory for runtime comparisons because the C++ baseline is built with `-O3`.

`--bootstrap-toolchains` installs missing Go and Rust toolchains under the benchmark build directory; that directory is a local build artifact and is not tracked.

The report directory contains:

- `metadata.json`
- `results.json`
- `benchmarks.csv`
- `summary.md`

`summary.md` includes normalized `Sleep perf` and `Noop perf` columns. Each workload is normalized independently: the best runtime is `1.00x`, and the others show their relative score against that best result. It also includes a C++ stackless coroutine parity section for Styio. The default route runs each runtime five times and reports medians; `results.json` keeps the successful samples for debugging noisy microbenchmarks.

The generated temporary C++/Go/Rust sources and binaries live under `build/async-runtime-release/async-runtime-work/<run-id>`, not inside the report.
