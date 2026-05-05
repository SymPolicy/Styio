# Async Runtime Benchmark Report

- Run ID: `20260505T051134Z-async-runtime`
- Host: `linux-aarch64`
- Tasks: `4` sleep tasks x `160ms`, `10000` no-op tasks, `4` workers/procs

| Language | Runtime | Status | Sleep seq ms | Sleep parallel ms | Speedup | Noop total us | Noop us/task | Toolchain / reason |
|---|---|---|---:|---:|---:|---:|---:|---|
| styio | styio_task_scheduler | ok | 647 | 161 | 4.019 | 13815 | 1.381 | repo runtime target |
| cpp | cpp_stackless_coroutine | ok | 646 | 160 | 4.037 | 9836 | 0.984 | Debian clang version 18.1.8 (18+b1) |
| go | goroutine | ok | 642 | 160 | 4.013 | 4621 | 0.462 | go version go1.26.2 linux/arm64 |
| rust | tokio_multi_thread | ok | 648 | 165 | 3.927 | 2979 | 0.298 | rustc 1.95.0 (59807616e 2026-04-14); cargo 1.95.0 (f2d3ce0bd 2026-03-21); tokio 1.52.2 |

## Interpretation

- `sleep` measures whether the runtime actually overlaps blocked tasks; speedup near the worker count indicates real scheduling instead of eager evaluation.
- `noop` measures submit/wait/release overhead for a large fanout of trivial tasks.
- C++ uses C++20 stackless coroutine frames with `co_await` and a small scheduler, Go uses goroutines with `GOMAXPROCS`, Rust uses Tokio's multi-thread runtime, and Styio uses the repository task scheduler target.
- `--bootstrap-toolchains` installs missing Go/Rust toolchains under the build directory; this keeps comparison runs reproducible on machines without system Go or Rust.
