# `tests/soak` — 单线程 Soak 框架（D.1）

本目录提供可扩展的单线程压力测试骨架，默认以 `soak_smoke` 级别运行，保障 PR 可控时长；可通过环境变量放大为长跑。

## 用例

- `StyioSoakSingleThread.TokenizerIngestionLoop`
  - 高频词法摄入循环（长输入 + 多轮 tokenization）
- `StyioSoakSingleThread.FileHandleLifecycleLoop`
  - 文件句柄长生命周期循环（open/read/rewind/close）
- `StyioSoakSingleThread.FileHandleMemoryGrowthBound`
  - 文件句柄循环的 RSS 增长阈值守卫（防回归泄漏）
- `StyioSoakSingleThread.ConcatMemoryGrowthBound`
  - 字符串拼接/释放循环的 RSS 增长阈值守卫（内存安全 Safety）
- `StyioSoakSingleThread.InvalidHandleDiagnosticsLoop`
  - 非零非法句柄高频误用诊断循环（rewind/read/write 置错，close 保持 no-op）
- `StyioSoakSingleThread.StreamProgramLoop`
  - M6 流式程序重复执行回归（`t02_running_max`）
- `StyioSoakSingleThread.StateInlineHelperProgramLoop`
  - 单参数 state helper（直返 `StateDecl`）在 pulse 体中调用的长跑回归（输出稳定 `1/3/6`）
- `StyioSoakSingleThread.StateInlineMatchCasesProgramLoop`
  - 单参数 state helper 使用 `?= { ... }` 更新表达式的长跑回归（输出稳定 `10/12/15`）
- `StyioSoakSingleThread.StateInlineInfiniteProgramLoop`
  - 单参数 state helper 使用 `[...]`（`InfiniteAST`）更新表达式的长跑回归（输出稳定 `0/0`）

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
ctest --test-dir build -L soak_deep --output-on-failure
```

`soak_deep` 档位由 `tests/CMakeLists.txt` 注入放大量参数：

- `STYIO_SOAK_LEXER_ITERS=5000`
- `STYIO_SOAK_INGEST_LINES=4096`
- `STYIO_SOAK_FILE_ITERS=12000`
- `STYIO_SOAK_FILE_LINES=256`
- `STYIO_SOAK_MEM_ITERS=8000`
- `STYIO_SOAK_MEM_FILE_LINES=128`
- `STYIO_SOAK_RSS_GROWTH_LIMIT_KIB=98304`
- `STYIO_SOAK_CONCAT_ITERS=6000`
- `STYIO_SOAK_CONCAT_CHAIN=16`
- `STYIO_SOAK_CONCAT_SEG_BYTES=128`
- `STYIO_SOAK_CONCAT_RSS_GROWTH_LIMIT_KIB=98304`
- `STYIO_SOAK_INVALID_HANDLE_ITERS=120000`
- `STYIO_SOAK_STREAM_ITERS=1500`
- `STYIO_SOAK_STATE_INLINE_ITERS=1500`
- `STYIO_SOAK_STATE_MATCH_ITERS=1500`
- `STYIO_SOAK_STATE_INFINITE_ITERS=1500`

说明：PR/CI 默认跑 `soak_smoke`，nightly 跑 `soak_deep`。

## 失败最小化与回归模板（D.5）

当 `soak_deep` 失败时，先用二分脚本找最小失败阈值，再沉淀回归样本：

```bash
./scripts/soak-minimize.sh \
  --test StyioSoakSingleThread.FileHandleMemoryGrowthBound \
  --var STYIO_SOAK_MEM_ITERS \
  --low 100 \
  --high 8000 \
  --extra-env "STYIO_SOAK_MEM_FILE_LINES=128;STYIO_SOAK_RSS_GROWTH_LIMIT_KIB=98304"
```

非法句柄诊断长跑回归可用：

```bash
./scripts/soak-minimize.sh \
  --test StyioSoakSingleThread.InvalidHandleDiagnosticsLoop \
  --var STYIO_SOAK_INVALID_HANDLE_ITERS \
  --low 2000 \
  --high 120000
```

产物落在 `tests/soak/regressions/<timestamp>-<case>/`。
回归记录模板见：

- `tests/soak/REGRESSION-TEMPLATE.md`
- `tests/soak/regressions/README.md`
