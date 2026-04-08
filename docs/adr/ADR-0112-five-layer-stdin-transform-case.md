# ADR-0112: Five-Layer Pipeline 增加 Stdin Transform Case

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0112: Five-Layer Pipeline 增加 Stdin Transform Case.

**Last updated:** 2026-04-08

## Context

`ADR-0111` 已把 `@stdin >> #(line) => { line >> @stdout }` 纳入 five-layer golden，确认了 stdin line iterator 与 stdout 写回的基础链路。

但 `M10` 的更高价值场景不是“原样 echo”，而是 iterator block 内先做算术绑定，再把结果写回 stdout：

```styio
@stdin >> #(line) => {
    result = line * 2
    result >> @stdout
}
```

如果没有这条 golden，`@stdin` 路径上的 string-to-int 转换、block 内 bind、以及 stdout 写回之间的配合仍只靠里程碑 stdout 回归覆盖，缺少 AST/StyioIR/LLVM IR 三层中间表示约束。

## Decision

新增 five-layer case `tests/pipeline_cases/p13_stdin_transform/`：

1. `input.styio` 固定为 `M10` 的 stdin transform 样例。
2. `stdin.txt` 作为 L5 子进程标准输入 fixture，内容为 `3/4/5`。
3. `expected/stdout.txt` 冻结运行结果 `6/8/10`。
4. `expected/ast.txt` 冻结 typed AST：
   - `@stdin` line iterator
   - block 内 `result = line * 2`
   - `result >> @stdout`
5. `expected/styio_ir.txt` 与 `expected/llvm_ir.txt` 冻结当前 lowering 形状，尤其是：
   - `styio.ir.stdin_line_iter`
   - `styio_cstr_to_i64`
   - `mul i64`
   - stdout `print_at/print_i64/print_done` basic blocks
6. `tests/styio_test.cpp` 新增 `StyioFiveLayerPipeline.P13_stdin_transform`。

## Consequences

正面：

1. `M10` 的 stdin 路径不再只冻结“直接 echo”，而是覆盖实际变换逻辑。
2. 未来若 parser、type inference、IR lowering 或 stdout codegen 重新整理，`p13` 会第一时间指出中间层契约漂移。
3. `stdin.txt` fixture 约定得到复用，stdin 类 five-layer cases 的维护方式保持一致。

负面：

1. 该 golden 固定了当前 `line * 2` 通过 `styio_cstr_to_i64` 降为 `i64` 的实现形状；后续若引入新的字符串数值转换抽象，需要同步更新 `llvm_ir.txt`。
2. `StyioIR` 当前仍只打印 `stdin_line_iter` 的摘要，不展开 block 内 bind/write 细节；若 `StyioRepr` 强化，需要同步更新 golden。
