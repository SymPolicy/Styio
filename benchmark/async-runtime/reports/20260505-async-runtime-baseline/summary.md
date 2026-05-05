# Async Runtime Benchmark Report

- Run ID: `20260505T061441Z-async-runtime`
- Host: `linux-aarch64`
- Tasks: `4` sleep tasks x `160ms`, `100000` no-op tasks, `4` workers/procs
- Repeats: `5` per runtime, reported values are medians

| Language | Runtime | Status | Samples | Sleep seq ms | Sleep parallel ms | Speedup | Sleep perf | Noop total us | Noop us/task | Noop perf | Toolchain / reason |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| styio | styio_task_scheduler | ok | 5 | 649.000 | 164.000 | 3.957 | 0.98x | 48984.000 | 0.490 | 0.72x | repo runtime target (Release; clang++-18) |
| cpp | cpp_stackless_coroutine | ok | 5 | 649.000 | 161.000 | 4.031 | 0.99x | 51580.000 | 0.516 | 0.68x | Debian clang version 18.1.8 (18+b1) |
| go | goroutine | ok | 5 | 649.000 | 160.000 | 4.056 | 1.00x | 35224.000 | 0.352 | 1.00x | go version go1.26.2 linux/arm64 |
| rust | tokio_multi_thread | ok | 5 | 649.000 | 163.000 | 3.982 | 0.98x | 35134.000 | 0.351 | 1.00x | rustc 1.95.0 (59807616e 2026-04-14); cargo 1.95.0 (f2d3ce0bd 2026-03-21); tokio 1.52.2 |

## Interpretation

- `sleep` measures whether the runtime actually overlaps blocked tasks; speedup near the worker count indicates real scheduling instead of eager evaluation.
- `noop` measures submit/wait/release overhead for a large fanout of trivial tasks.
- `Samples` records successful repeats; all table metrics are median values to avoid single-run microbenchmark noise.
- `Sleep perf` and `Noop perf` normalize each workload independently; the best runtime is `1.00x`, and the others show their relative performance against that best result.
- C++ uses C++20 stackless coroutine frames with `co_await` and a small scheduler, Go uses goroutines with `GOMAXPROCS`, Rust uses Tokio's multi-thread runtime, and Styio uses the repository task scheduler target.
- `--bootstrap-toolchains` installs missing Go/Rust toolchains under the build directory; this keeps comparison runs reproducible on machines without system Go or Rust.

## C++ Stackless Parity

- Styio no-op vs C++ stackless coroutine: `1.05x` (`0.490` us/task vs `0.516` us/task).
- Styio sleep overlap vs C++ stackless coroutine: `0.98x` (`3.957` speedup vs `4.031`).
