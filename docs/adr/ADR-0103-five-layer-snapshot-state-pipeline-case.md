# ADR-0103: Five-Layer Pipeline 增加 Snapshot/State Case

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0103: Five-Layer Pipeline 增加 Snapshot/State Case.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0101` 之后，five-layer pipeline 的 L2 parser 已经切到 `nightly`，但已落地的 cases 仍偏向基础表达式与文件 I/O：

1. `p01_print_add`
2. `p02_simple_func`
3. `p03_write_file`
4. `p04_read_file`

这还缺一类高价值路径：`snapshot decl + state ref + iterator block + compound accumulation`。这条链路同时触达：

1. `nightly` parser 对 `@[...]` / `$state` / iterator block 的解析
2. typed AST 中 `snapshot/state-ref` 的形状
3. StyioIR 对 snapshot/foreach/print 的 lowering
4. LLVM IR 对 `styio_read_file_i64line` 与循环累加的代码生成

## Decision

1. 新增 five-layer case：
   - `tests/pipeline_cases/p05_snapshot_accum/`
2. case 输入固定为：
   - 从 `@file{"/tmp/styio_pipeline_factor.txt"}` 读入 snapshot `factor`
   - 遍历 `[1, 2, 3]`
   - 在 block 中计算 `prod = x * $factor`
   - 用 `sum += prod` 做累加
   - 最终 `>_(sum)` 输出 `12`
3. 测试端在运行前准备 `/tmp/styio_pipeline_factor.txt`，避免依赖 CTest 当前工作目录。
4. golden 固定五层产物：
   - `tokens.txt`
   - `ast.txt`
   - `styio_ir.txt`
   - `llvm_ir.txt`
   - `stdout.txt`

## Alternatives

1. 继续只覆盖基础表达式和资源读写：
   - 拒绝。无法直接观察 snapshot/state 相关回归。
2. 只加一个 stdout golden，不做 five-layer：
   - 拒绝。这样看不到 nightly parser、typed AST 和 IR 层的具体漂移。
3. 使用仓库相对路径输入文件：
   - 拒绝。测试工作目录不稳定，恢复脚本和 CTest 下容易出现假失败。

## Consequences

1. nightly parser 在 `snapshot/state-ref` 这条链路上第一次进入 five-layer golden。
2. 后续若 `snapshot decl`、`state ref`、iterator block 或累加 lowering 回归，失败会直接定位到具体层。
3. pipeline 覆盖面从基础表达式/函数/I-O 扩展到状态式流处理，为后续继续补 M7/M6 类 case 提供模板。
