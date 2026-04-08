# ADR-0085: Parser 默认引擎切换到 New（保留 Legacy 回退）

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0085: Parser 默认引擎切换到 New（保留 Legacy 回退）.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

E.6 门槛阶段已经具备以下前提：

1. `--parser-shadow-compare` 在 M1/M2 样例上持续稳定通过；
2. `new` 路径对非子集语法采用保守回退到 `legacy`；
3. `--parser-engine=legacy|new` 双轨开关已长期存在并被回归覆盖。

但 CLI 默认仍是 `legacy`，与“逐步切主行为”目标不一致。

## Decision

1. 将 CLI `--parser-engine` 默认值从 `legacy` 切换为 `new`。
2. 维持 `--parser-engine=legacy` 显式回退能力不变。
3. 增加回归：
   - `StyioParserEngine.DefaultEngineIsNewInShadowArtifact`
   - 通过 `--parser-shadow-compare --parser-shadow-artifact-dir` 校验默认主引擎字段为 `new`。

## Alternatives

1. 继续保持默认 `legacy`，仅文档声明推荐 `new`：
   - 拒绝。无法形成真实主路径流量，E.6 目标无法闭环。
2. 直接移除 `legacy` 选项：
   - 拒绝。当前仍需保留应急回退路径，属于 E.7 稳定窗口后的动作。

## Consequences

1. 主路径切到 `new`，但在非子集语法下仍由 `parse_main_block_new_shadow` 保守回退，行为风险可控。
2. 兼容性回退仍可通过 `--parser-engine=legacy` 触发。
3. 默认引擎契约被自动化测试冻结，防止后续无意回退为 `legacy`。
