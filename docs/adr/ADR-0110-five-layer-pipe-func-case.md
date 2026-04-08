# ADR-0110: Five-Layer Pipeline 增加 Pipe Function Case

- Status: Accepted
- Date: 2026-04-08

## Context

在 `ADR-0109` 之后，five-layer pipeline 已覆盖简单函数、显式/auto-detect 文件读取、redirect、full pipeline 以及 instant pull。但还缺一条把“函数定义”和“iterator body 内函数调用”连到同一条流处理链上的组合 case。

`tests/milestones/m5/t08_pipe_func.styio` 已经证明这条路径是有效语言行为，但 five-layer 还没有冻结它的 AST/IR/LLVM 形状。

## Decision

1. 新增 five-layer case：
   - `tests/pipeline_cases/p11_pipe_func/`
2. case 语义固定为：
   - `# double_it := (x: i32) => x * 2`
   - `f <- @file{"/tmp/styio_pipeline_numbers.txt"}`
   - `f >> #(line) => { >_(double_it(line)) }`
3. 测试端在运行前准备 `/tmp/styio_pipeline_numbers.txt`，运行后清理该输入文件。
4. golden 固定五层产物，覆盖：
   - simple function 定义
   - `handle.acquire` + `file_line_iter`
   - iterator body 内 `call`
   - LLVM 中 `styio_cstr_to_i64` 后接用户函数调用的 lowering

## Alternatives

1. 继续只靠 `p02_simple_func` 加 `p04_read_file` 的组合理解：
   - 拒绝。两条独立 case 不能冻结“函数调用处于 iterator body 内”这条组合风险面。
2. 继续只靠 M5 `t08_pipe_func` 端到端 stdout：
   - 拒绝。看不到 AST/IR/LLVM 层漂移。
3. 先做 five-layer 参数化框架再补 case：
   - 暂不采用。当前 checkpoint 更重要的是先把风险面冻结下来。

## Consequences

1. five-layer pipeline 第一次覆盖“函数调用位于流处理 block 内”的组合路径。
2. 后续若 parser、type-check、IR lowering 或 codegen 在这条链路上漂移，回归会直接定位到具体层。
3. five-layer 对 M5 的覆盖从“资源读写”扩展到“资源流 + 用户函数调用”。
