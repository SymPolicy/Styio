# ADR-0113: Five-Layer Pipeline 增加 Stdin Pull Case

## Context

`ADR-0111` 与 `ADR-0112` 已经把 `@stdin` 的 line-iterator 路径纳入 five-layer golden，分别覆盖了“原样写回”和“block 内算术变换”。

但 `M10` 还有另一条独立核心语法 `(<< @stdin)`，也就是 instant pull。它的 lowering 形状与 iterator 完全不同：

```styio
x = (<< @stdin)
>_(x)
```

如果不单独冻结这条路径，`SIOStdStreamPull` / `styio_stdin_read_line()` / `styio_cstr_to_i64()` 之间的契约仍只由里程碑 stdout 测试间接覆盖，无法在 AST/StyioIR/LLVM IR 三层快速定位漂移。

## Decision

新增 five-layer case `tests/pipeline_cases/p14_stdin_pull/`：

1. `input.styio` 固定为 `x = (<< @stdin); >_(x)`。
2. `stdin.txt` 作为 L5 子进程标准输入 fixture，内容为 `42`。
3. `expected/stdout.txt` 冻结运行结果 `42`。
4. `expected/ast.txt` 冻结 typed AST：
   - `flex bind x = instant.pull(@stdin)`
   - `print(x)`
5. `expected/styio_ir.txt` 与 `expected/llvm_ir.txt` 冻结当前 lowering 形状，尤其是：
   - `styio_stdin_read_line`
   - `styio_cstr_to_i64`
   - `flex_bind`
   - stdout `print_at/print_i64/print_done` basic blocks
6. `tests/styio_test.cpp` 新增 `StyioFiveLayerPipeline.P14_stdin_pull`。

## Consequences

正面：

1. `@stdin` 的两条主语法现在都进入 five-layer：iterator 和 instant pull。
2. 未来若 stdin runtime helper、type inference 或 print lowering 变化，`p14` 能比 stdout-only 回归更早、更精确地指出是哪一层漂移。
3. `stdin.txt` fixture 约定继续复用，stdin 类 five-layer cases 的维护方式保持一致。

负面：

1. 该 golden 固定了当前 instant pull 默认走 `cstr -> i64` 的实现形状；若未来引入更显式的类型约束，需要同步更新 `llvm_ir.txt`。
2. `StyioIR` 当前只显示 `flex_bind` / `print` 摘要，不展开 `SIOStdStreamPull` 细节；若 `StyioRepr` 增强，需要同步更新 golden。
