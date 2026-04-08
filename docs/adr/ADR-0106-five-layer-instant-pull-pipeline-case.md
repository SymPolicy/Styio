# ADR-0106: Five-Layer Pipeline 增加 Instant Pull Case

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0106: Five-Layer Pipeline 增加 Instant Pull Case.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-08

## Context

在 `ADR-0105` 之后，five-layer pipeline 已覆盖 `snapshot/state` 与 `stream zip + file iterator`。但 `M7` 还有一条独立且高价值的 parser/runtime 路径没有进入 five-layer：

1. `(<< @file{...})` 即 `instant pull`

这条链路与普通 `@file >> #(line)` 不同，它把资源读取直接嵌进表达式，因此能约束：

1. `nightly` parser 对 `instant.pull` AST 的构造
2. 表达式级资源读取的 typed AST 形状
3. `styio_read_file_i64line(...)` 在算术表达式中的 lowering

## Decision

1. 新增 five-layer case：
   - `tests/pipeline_cases/p07_instant_pull/`
2. case 语义固定为：
   - `x = 1`
   - `result = x + (<< @file{"/tmp/styio_pull_value.txt"})`
   - `>_(result)`
3. 测试端在运行前准备 `/tmp/styio_pull_value.txt`，使 case 不依赖工作目录。
4. golden 固定五层产物，覆盖 `instant.pull` AST 与表达式级 file-read LLVM lowering。

## Alternatives

1. 继续只用 M7 stdout golden 观察 instant pull：
   - 拒绝。定位粒度不够，无法看到 AST/IR 漂移。
2. 用 snapshot 或 iterator 间接覆盖这条路径：
   - 拒绝。那会绕过 `instant.pull` 作为表达式节点的核心差异。
3. 先修复 typed AST 中 `result` 仍显示 `undefined` 再加 case：
   - 暂不采用。当前 case 先冻结现有真实行为，后续若类型推断改进，再单独更新 golden 与 ADR。

## Consequences

1. `instant pull` 第一次进入 nightly-first five-layer golden。
2. 后续若 parser、type-infer 或 codegen 在表达式级 file-read 上回归，会直接落到具体层。
3. five-layer pipeline 已经覆盖 `M7` 的三条核心路径：
   - snapshot/state
   - zip/file-stream
   - instant pull
