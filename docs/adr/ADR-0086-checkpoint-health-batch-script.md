# ADR-0086: Checkpoint Health 批量验证脚本

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0086: Checkpoint Health 批量验证脚本.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

随着 C.5/D.3/E.6 的并行推进，恢复演练需要跨多个维度手工执行命令：

1. parser 默认引擎契约；
2. state-inline 诊断回归；
3. state-inline soak smoke/deep；
4. `styio_pipeline` 与 `security` 标签；
5. 可选 ASan/UBSan 目标回归。

手工命令链长且容易漏项，不利于“随时可中断、随时可恢复”。

## Decision

1. 新增 `scripts/checkpoint-health.sh` 作为统一入口：
   - 默认执行 normal build + 关键回归 + soak + pipeline/security；
   - 提供 `--no-asan` 快速模式；
   - 提供 `--build-dir` / `--asan-build-dir` 参数用于自定义构建目录。
2. state-inline 相关 deep soak (`program/matchcases/infinite`) 纳入该脚本固定清单。
3. 输出统一前缀 `[checkpoint-health]`，便于 CI/人工日志快速定位阶段。

## Alternatives

1. 继续依赖 history 文档中的分散命令：
   - 拒绝。恢复路径过长且遗漏风险高。
2. 将全部逻辑塞入 CI 工作流：
   - 拒绝。本地恢复同样需要一键入口，不应只依赖远端 CI。

## Consequences

1. 中断恢复从“多条手工命令”收敛为一条脚本命令。
2. D.5 的恢复演练可重复性提升，且能稳定覆盖 parser/state-inline 高风险路径。
3. 后续新增高风险回归只需扩展同一脚本，维护成本更低。
