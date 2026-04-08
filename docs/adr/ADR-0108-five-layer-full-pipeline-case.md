# ADR-0108: Five-Layer Pipeline 增加 Full Pipeline Case

- Status: Accepted
- Date: 2026-04-08

## Context

在 `ADR-0107` 之后，five-layer pipeline 已覆盖：

1. 基础表达式
2. 简单函数
3. 文件写入
4. 文件读取 iterator
5. snapshot/state
6. zip/file-stream
7. instant pull
8. redirect/file

但仍缺一条真正把 `file iterator`、block 内 `flex bind`、以及 `resource.write` 连成一条链的组合型 case。`tests/milestones/m7/t10_full_pipeline.styio` 已证明这条语义链路重要，但 five-layer 还没有对应 golden。

## Decision

1. 新增 five-layer case：
   - `tests/pipeline_cases/p09_full_pipeline/`
2. case 语义固定为：
   - `@file{"/tmp/styio_pipeline_stream_input.txt"} >> #(x) => { ... }`
   - body 内执行 `result = x * 2`
   - 然后 `result << @file{"/tmp/styio_pipeline_stream_output.txt"}`
3. 测试端在运行前准备 `/tmp/styio_pipeline_stream_input.txt`，运行前清理输出文件，运行后断言 `/tmp/styio_pipeline_stream_output.txt` 内容为 `20/40/60`。
4. fixture 选用多位数字 `10/20/30`，避免把已观察到的单数字写回异常混入本 checkpoint。
5. golden 固定五层产物，覆盖：
   - `styio.ast.iterator`
   - block 内 `styio.ast.bind.flex`
   - `styio.ast.resource.write`
   - `styio.ir.file_line_iter`
   - 对应 LLVM lowering 与文件副作用输出

## Alternatives

1. 继续只靠 `m7_t10_full_pipeline` 端到端副作用测试：
   - 拒绝。看不到 AST/IR/LLVM 层漂移。
2. 把 full pipeline 合并到已有 `p04_read_file` 或 `p03_write_file`：
   - 拒绝。单点 I/O case 与组合链路的定位粒度不同。
3. 直接冻结单数字输入：
   - 暂不采用。当前已观察到单数字写回异常，需要单独立项，而不是污染本 checkpoint 的基线。

## Consequences

1. five-layer pipeline 第一次覆盖“读取 + 绑定 + 写回”的组合型资源流路径。
2. 后续若 nightly parser、AST analyzer、IR lowering 或 codegen 在这条链路上漂移，回归会直接定位到具体层。
3. 单数字写回异常被显式留在风险边界之外，后续需要单独补一个 safety regression checkpoint。
