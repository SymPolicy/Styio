# Async Runtime Benchmark Report

- Run ID: `20260505T074418Z-async-runtime`
- Host: `linux-aarch64`
- Case: `baseline` (median performance comparison against C++ stackless coroutine, goroutine, and Tokio)
- Harness: `pytest-compatible black-box runner` over `subprocess`
- Runtimes: `styio, cpp, go, rust`
- Required runtimes: `none`
- Tasks: `4` sleep tasks x `160ms`, `100000` no-op tasks, `4` workers/procs
- Repeats: `5` per runtime, reported values are medians

| Language | Runtime | Status | Samples | Sleep seq ms | Sleep parallel ms | Speedup | Sleep perf | Noop total us | Noop us/task | Noop perf | Toolchain / reason |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| styio | styio_task_scheduler | ok | 5 | 660.000 | 166.000 | 3.976 | 0.99x | 54123.000 | 0.541 | 0.61x | repo runtime target (Release; clang++-18) |
| cpp | cpp_stackless_coroutine | ok | 5 | 659.000 | 165.000 | 3.994 | 0.99x | 59097.000 | 0.591 | 0.56x | Debian clang version 18.1.8 (18+b1) |
| go | goroutine | ok | 5 | 660.000 | 164.000 | 4.024 | 1.00x | 33258.000 | 0.333 | 1.00x | go version go1.26.2 linux/arm64 |
| rust | tokio_multi_thread | ok | 5 | 655.000 | 163.000 | 4.018 | 1.00x | 36107.000 | 0.361 | 0.92x | rustc 1.95.0 (59807616e 2026-04-14); cargo 1.95.0 (f2d3ce0bd 2026-03-21); tokio 1.52.2 |

## Interpretation

- `sleep` measures whether the runtime actually overlaps blocked tasks; speedup near the worker count indicates real scheduling instead of eager evaluation.
- `noop` measures submit/wait/release overhead for a large fanout of trivial tasks.
- `Samples` records successful repeats; all table metrics are median values to avoid single-run microbenchmark noise.
- `Sleep perf` and `Noop perf` normalize each workload independently; the best runtime is `1.00x`, and the others show their relative performance against that best result.
- The runner is intentionally pytest-compatible: each runtime is a subprocess black box, and pytest can assert the generated JSON/CSV contract without embedding language-specific unit-test frameworks.
- C++ uses C++20 stackless coroutine frames with `co_await` and a small scheduler built with Clang by default, Go uses goroutines with `GOMAXPROCS`, Rust uses Tokio's multi-thread runtime, and Styio uses the repository task scheduler target.
- `--bootstrap-toolchains` installs missing Go/Rust toolchains under the build directory; this keeps comparison runs reproducible on machines without system Go or Rust.

## C++ Stackless Parity

- Styio no-op vs C++ stackless coroutine: `1.09x` (`0.541` us/task vs `0.591` us/task).
- Styio sleep overlap vs C++ stackless coroutine: `1.00x` (`3.976` speedup vs `3.994`).
