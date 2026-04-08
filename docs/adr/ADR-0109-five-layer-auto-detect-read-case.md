# ADR-0109: Five-Layer Pipeline 增加 Auto-Detect Read Case

- Status: Accepted
- Date: 2026-04-08

## Context

在 `ADR-0108` 之后，five-layer pipeline 已覆盖常规 `@file{...}` 读取、写入、redirect，以及一条组合型 full pipeline。但 `@{...}` 这条 auto-detect 资源入口仍只在 M5 端到端测试中通过 stdout 观察，没有冻结 `auto_detect=true` 和 `handle_acquire auto=1` 这组中间层语义。

## Decision

1. 新增 five-layer case：
   - `tests/pipeline_cases/p10_auto_detect_read/`
2. case 语义固定为：
   - `f <- @{"/tmp/styio_pipeline_auto_input.txt"}`
   - `f >> #(line) => { >_(line) }`
3. 测试端在运行前准备 `/tmp/styio_pipeline_auto_input.txt`，运行后清理该输入文件。
4. golden 固定五层产物，覆盖：
   - `styio.ast.resource.file { auto_detect: true }`
   - `styio.ir.handle_acquire { auto=1, ... }`
   - `styio_file_open_auto` LLVM lowering

## Alternatives

1. 继续只靠 M5 `t05_auto_detect` 的 stdout golden：
   - 拒绝。无法观察 AST/IR/LLVM 层面的 auto-detect 标志漂移。
2. 把 `@{...}` 与 `@file{...}` 合并到同一个 five-layer case：
   - 拒绝。两条路径的中间层语义不同，合并后定位不清。
3. 直接扩 `p04_read_file` 改成参数化：
   - 暂不采用。当前 checkpoint 目标是把 auto-detect 作为独立风险面冻结，而不是先做测试框架抽象。

## Consequences

1. `@{...}` auto-detect 路径第一次进入 nightly-first five-layer golden。
2. 后续若 parser、AST analyzer、IR lowering 或 codegen 在 auto-detect 标志上漂移，回归会直接定位到具体层。
3. five-layer pipeline 对资源读取的覆盖从“显式 file”扩展到“auto-detect file”。
