# `tests/soak` — 单线程 Soak 框架（D.1）

本目录提供可扩展的单线程压力测试骨架，默认以 `soak_smoke` 级别运行，保障 PR 可控时长；可通过环境变量放大为长跑。

## 用例

- `StyioSoakSingleThread.TokenizerIngestionLoop`
  - 高频词法摄入循环（长输入 + 多轮 tokenization）
- `StyioSoakSingleThread.FileHandleLifecycleLoop`
  - 文件句柄长生命周期循环（open/read/rewind/close）
- `StyioSoakSingleThread.StreamProgramLoop`
  - M6 流式程序重复执行回归（`t02_running_max`）

## C ABI 指针约定

- `styio_file_read_line` 返回借用指针（线程本地缓冲），不可 `styio_free_cstr`。
- `styio_strcat_ab` 返回堆内存，必须配对 `styio_free_cstr`。

## 运行

```bash
cmake -S . -B build
cmake --build build --target styio_soak_test -j8
ctest --test-dir build -L soak_smoke --output-on-failure
```

## 放大量（本地/夜间）

```bash
STYIO_SOAK_LEXER_ITERS=5000 \
STYIO_SOAK_INGEST_LINES=4000 \
STYIO_SOAK_FILE_ITERS=5000 \
STYIO_SOAK_FILE_LINES=256 \
STYIO_SOAK_STREAM_ITERS=500 \
./build/bin/styio_soak_test --gtest_filter='StyioSoakSingleThread.*'
```

说明：PR/CI 默认不设置上述变量，维持 smoke 时长。
