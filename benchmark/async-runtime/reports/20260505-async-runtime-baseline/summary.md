# Async Runtime Benchmark Report

- Run ID: `20260505T041640Z-async-runtime`
- Host: `linux-aarch64`
- Tasks: `4` sleep tasks x `160ms`, `10000` no-op tasks, `4` workers/procs

| Language | Runtime | Status | Sleep seq ms | Sleep parallel ms | Speedup | Noop total us | Noop us/task | Toolchain / reason |
|---|---|---|---:|---:|---:|---:|---:|---|
| styio | styio_task_scheduler | ok | 653 | 166 | 3.934 | 11411 | 1.141 | repo runtime target |
| cpp | cpp_thread_pool | ok | 646 | 164 | 3.939 | 13658 | 1.366 | Debian clang version 18.1.8 (18+b1) |
| go | goroutine | ok | 651 | 160 | 4.069 | 3771 | 0.377 | go version go1.26.2 linux/arm64 |
| rust | rust_std_worker_pool | ok | 655 | 162 | 4.043 | 3115 | 0.311 | rustc 1.95.0 (59807616e 2026-04-14) |

## Interpretation

- `sleep` measures whether the runtime actually overlaps blocked tasks; speedup near the worker count indicates real scheduling instead of eager evaluation.
- `noop` measures submit/wait/release overhead for a large fanout of trivial tasks.
- C++ uses a fixed worker pool, Go uses goroutines with `GOMAXPROCS`, Rust uses a standard-library worker pool, and Styio uses the repository task scheduler target.
- `--bootstrap-toolchains` installs missing Go/Rust toolchains under the build directory; this keeps comparison runs reproducible on machines without system Go or Rust.
