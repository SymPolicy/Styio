# ADR-0105: Five-Layer Pipeline 增加 Zip/File Stream Case

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0103` 之后，five-layer pipeline 已覆盖：

1. 基础表达式
2. 简单函数
3. 文件写入
4. 文件读取 iterator
5. snapshot/state 累加

但还缺少一条对 `M7` 很关键的路径：`resource file iterator + stream zip + arithmetic print`。现有 shadow/parity 测试能证明 nightly/legacy 一致，却不能把 typed AST、StyioIR 和 LLVM IR 同时固定下来。

## Decision

1. 新增 five-layer case：
   - `tests/pipeline_cases/p06_zip_files/`
2. case 语义固定为：
   - 从 `/tmp/styio_zip_a.txt` 与 `/tmp/styio_zip_b.txt` 读取两条文件流
   - 用 `>> #(a) & ... >> #(b)` 形成 `stream zip`
   - 在 body 中执行 `>_(a - b)`
3. 测试端在运行前准备两个 `/tmp` 输入文件，避免依赖工作目录。
4. golden 固定五层产物，覆盖：
   - `stream.zip` AST 形状
   - `styio.ir.stream_zip`
   - LLVM `zip_ff_*` lowering

## Alternatives

1. 只保留 `m7` 端到端 stdout golden：
   - 拒绝。定位粒度太粗，看不到 nightly parser/IR 漂移。
2. 先加 zip list，再晚点加 zip file：
   - 暂不采用。仓库当前更缺的是资源流 zip，而不是 list zip。
3. 使用仓库内相对路径数据文件：
   - 拒绝。five-layer case 应该显式准备外部输入，避免工作目录敏感。

## Consequences

1. `stream zip + file iterator` 第一次进入 nightly-first five-layer golden。
2. 后续若 zip lowering、资源读取桥接或 AST 形状回归，失败会直接落到具体层。
3. five-layer pipeline 覆盖范围继续向 `M7` 实战路径推进，而不只停留在基础语法。
