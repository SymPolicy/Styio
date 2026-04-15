# `benchmark/` — Styio 基准与性能路线

这里是仓库里性能/基准相关内容的主入口。所有基准源码、性能脚本、soak 长跑与最小化工具都统一收口在这个目录下。

约束很简单：

- 这里是 benchmark/perf 的唯一权威入口
- 其他文档只保留摘要与跳转，不重复维护 workload 细节
- 变更 benchmark 覆盖面或执行路线时，只更新这里和 [`COVERAGE-MATRIX.md`](<user-home>/DevSpace/styio-org/styio/benchmark/COVERAGE-MATRIX.md:1)

## 目录

- `styio_soak_test.cpp`
  - 基准与单线程 soak 测试源码
- `CMakeLists.txt`
  - `styio_soak_test` 目标与 `soak_deep_*` CTest 注入
- `perf-route.sh`
  - 一键性能路线 + 结果归档
- `perf-report.py`
  - 解析原始日志并生成 `json/csv/markdown` 摘要
- `parser-shadow-suite-gate.sh`
  - parser shadow gate 脚本
- `soak-minimize.sh`
  - soak 失败二分最小化
- `REGRESSION-TEMPLATE.md`
  - 回归记录模板
- `COVERAGE-MATRIX.md`
  - 按编译阶段与模块切面的 benchmark 覆盖矩阵
- `regressions/`
  - 最小化失败工件目录
- `reports/`
  - 本地 benchmark 运行产物目录（默认不入库）

## 路线

### 快速

```bash
./benchmark/perf-route.sh --quick
```

### 标准

```bash
./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20
```

### 深跑

```bash
./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20 --deep-soak
```

这三条路线都会覆盖：

- compiler stage benchmark matrix（`tokenize/parse/type/lower/llvm_ir`）
- compiler micro benchmark matrix（`lexer/parser/type/lower/llvm` 热点切面）
- full-stack workload matrix（CLI wall-clock）
- compiler error-path benchmark matrix（`lex/parse/type/runtime` 失败路径）
- parser engine 回归
- pipeline guard rail
- parser/security guard rail
- parser shadow gates
- soak smoke
- 可选 `soak_deep`

覆盖设计与补强方向见 [COVERAGE-MATRIX.md](<user-home>/DevSpace/styio-org/styio/benchmark/COVERAGE-MATRIX.md:1)。

每次运行默认会把产物写到 `benchmark/reports/<timestamp>/`，包括：

- `metadata.tsv`
- `sections.tsv`
- `logs/*.log`
- `results.json`
- `benchmarks.csv`
- `summary.md`

也可以显式指定：

```bash
./benchmark/perf-route.sh \
  --phase-iters 5000 \
  --micro-iters 5000 \
  --execute-iters 20 \
  --error-iters 50 \
  --label baseline \
  --out-dir benchmark/reports/first-full-baseline
```

## Compiler Stage Benchmark

这是 opt-in benchmark，默认不会进 `ctest -L soak_smoke`，需要显式设置迭代次数：

```bash
STYIO_SOAK_PHASE_BENCH_ITERS=5000 \
./build/bin/styio_soak_test \
  --gtest_filter=StyioSoakSingleThread.FrontendPhaseBreakdownReport
```

核心输出字段：

- `tokenize_us`
- `parse_us`
- `type_us`
- `lower_us`
- `llvm_ir_us`
- `parse_share_pct`
- `lower_share_pct`
- `llvm_ir_share_pct`
- `avg_token_arena_kib`
- `avg_ast_arena_kib`
- `rss_growth_kib`

当前 matrix 覆盖这些模块切面：

- `Scalar`: `scalar_core`
- `Bindings`: `bindings_chain`
- `Functions`: `function_block_body`
- `ControlFlow`: `control_match`
- `Collections`: `dict_heavy`
- `Resources`: `resource_file_io`
- `Streams`: `stdin_transform`, `stream_zip_files`
- `StateAndSeries`: `snapshot_state`, `series_window_avg`, `state_pulse_inline`
- `Topology`: `topology_ring`
- `Mixed`: `mixed_full_pipeline`（compile-only）

## Full-Stack Workload Matrix

这也是 opt-in benchmark，用来冻结 CLI 从解析到执行的总墙钟时间：

```bash
STYIO_SOAK_EXECUTE_BENCH_ITERS=20 \
./build/bin/styio_soak_test \
  --gtest_filter=StyioSoakSingleThread.FullStackWorkloadMatrixReport
```

核心输出字段：

- `cli_wall_us`

[`benchmark/perf-route.sh`](<user-home>/DevSpace/styio-org/styio/benchmark/perf-route.sh:1) 默认会一起执行；如果不传 `--execute-iters`，脚本会根据 `--phase-iters` 自动推导一个较小的执行迭代数。

## Compiler Error-Path Benchmarks

这是 opt-in benchmark，用来冻结失败路径的诊断成本，并验证错误类别和退出码保持稳定：

```bash
STYIO_SOAK_ERROR_BENCH_ITERS=50 \
./build/bin/styio_soak_test \
  --gtest_filter=StyioSoakSingleThread.CompilerErrorPathBenchmarksReport
```

当前第一批 error-path benchmark 覆盖：

- `lex.unterminated_block_comment`
- `parse.empty_match_cases`
- `type.final_then_flex_i64`
- `runtime.read_missing_file`

核心输出字段：

- `category`
- `error_us`
- `exit_code`
- `diagnostic_code`
- `avg_diag_bytes`

[`benchmark/perf-route.sh`](<user-home>/DevSpace/styio-org/styio/benchmark/perf-route.sh:1) 默认也会跑这一组；如果不传 `--error-iters`，脚本会根据 `--phase-iters` 自动推导一个较小但稳定的失败路径迭代数。

## Compiler Micro Benchmarks

这是比 stage matrix 更窄的切面基准，用来盯住热点模块本身，而不是整条 workload 的阶段占比：

```bash
STYIO_SOAK_MICRO_BENCH_ITERS=5000 \
./build/bin/styio_soak_test \
  --gtest_filter=StyioSoakSingleThread.CompilerMicroBenchmarksReport
```

当前第一批 micro benchmark 覆盖：

- `lexer.long_identifiers`
- `lexer.mixed_trivia`
- `parser.expr_scalar_chain`
- `parser.function_block_body`
- `parser.match_cases`
- `parser.iterator_resource_postfix`
- `type.dict_heavy`
- `type.snapshot_state`
- `lower.stream_zip`
- `lower.resource_io`
- `lower.state_pulse`
- `llvm.scalar_ir`
- `llvm.resource_ir`
- `llvm.state_ir`

核心输出字段：

- `focus`
- `focus_us`
- `avg_tokens`
- `avg_token_arena_kib`
- `avg_ast_arena_kib`
- `rss_growth_kib`

[`benchmark/perf-route.sh`](<user-home>/DevSpace/styio-org/styio/benchmark/perf-route.sh:1) 默认也会跑这一组；如果不传 `--micro-iters`，它默认继承 `--phase-iters`。

## 测试面

常驻 smoke / soak 回归：

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

Opt-in benchmark：

- `StyioSoakSingleThread.FrontendPhaseBreakdownReport`
  - 编译阶段 matrix benchmark，输出 `tokenize/parse/type/lower/llvm_ir` 单次耗时、阶段占比、arena 平均占用和 RSS 增长
- `StyioSoakSingleThread.CompilerMicroBenchmarksReport`
  - 编译器微基准 matrix，输出 `lexer/parser/type/lower/llvm` 热点切面的单次耗时与 arena/RSS 指标
- `StyioSoakSingleThread.FullStackWorkloadMatrixReport`
  - CLI end-to-end matrix benchmark，输出模块切面的单次墙钟耗时
- `StyioSoakSingleThread.CompilerErrorPathBenchmarksReport`
  - CLI 失败路径 matrix benchmark，输出 `lex/parse/type/runtime` 诊断的单次墙钟耗时、退出码与诊断码

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

`soak_deep` 档位由 [`benchmark/CMakeLists.txt`](<user-home>/DevSpace/styio-org/styio/benchmark/CMakeLists.txt:1) 注入放大量参数：

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
./benchmark/soak-minimize.sh \
  --test StyioSoakSingleThread.FileHandleMemoryGrowthBound \
  --var STYIO_SOAK_MEM_ITERS \
  --low 100 \
  --high 8000 \
  --extra-env "STYIO_SOAK_MEM_FILE_LINES=128;STYIO_SOAK_RSS_GROWTH_LIMIT_KIB=98304"
```

非法句柄诊断长跑回归可用：

```bash
./benchmark/soak-minimize.sh \
  --test StyioSoakSingleThread.InvalidHandleDiagnosticsLoop \
  --var STYIO_SOAK_INVALID_HANDLE_ITERS \
  --low 2000 \
  --high 120000
```

产物落在 `benchmark/regressions/<timestamp>-<case>/`。
回归记录模板见：

- `benchmark/REGRESSION-TEMPLATE.md`
- `benchmark/regressions/README.md`
