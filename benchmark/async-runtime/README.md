# Async Runtime Benchmark Framework

This directory contains the cross-runtime async benchmark route for the Styio task scheduler.

The route compares the same two workloads across:

- `styio_task_scheduler`: the repository runtime target.
- `cpp_stackless_coroutine`: a C++20 stackless coroutine scheduler built with coroutine frames and `co_await`.
- `goroutine`: Go goroutines with `GOMAXPROCS` pinned to the worker count.
- `tokio_multi_thread`: Rust Tokio's multi-thread runtime.

## Workloads

1. `sleep`: four `160ms` blocked tasks. This proves whether a runtime overlaps blocked work. A speedup near the worker count means the implementation is truly scheduled rather than eager.
2. `noop`: ten thousand trivial tasks. This exposes submit, wake, wait, and release overhead.

## Run

```bash
benchmark/async-runtime/run-async-bench.py \
  --build-dir build \
  --bootstrap-toolchains \
  --out-dir benchmark/async-runtime/reports/<run-id>
```

`--bootstrap-toolchains` installs missing Go and Rust toolchains under `build/async-runtime-toolchains`; that directory is a local build artifact and is not tracked.

The report directory contains:

- `metadata.json`
- `results.json`
- `benchmarks.csv`
- `summary.md`

The generated temporary C++/Go/Rust sources and binaries live under `build/async-runtime-work/<run-id>`, not inside the report.
