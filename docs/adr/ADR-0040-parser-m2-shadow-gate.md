# ADR-0040: Parser M2 Core Shadow Gate 显式化

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0040: Parser M2 Core Shadow Gate 显式化.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

M2 函数定义入口已纳入 NewParser 子集，并新增 `ShadowCompareAcceptsM2CoreSuite` 回归。但若只依赖 `styio_pipeline` 大标签，失败定位不够直接，且门槛可见性弱。

## Decision

1. 在 CI 增加显式步骤：
   - `Parser shadow gate (M2 core suite)`
   - 执行 `ctest --test-dir build -R '^StyioParserEngine.ShadowCompareAcceptsM2CoreSuite$'`
2. 维持既有 M1 gate 不变，形成 M1+M2 双 gate 结构。

## Alternatives

1. 继续依赖 `styio_pipeline` 间接覆盖。
2. 等 M2 全量覆盖后再加显式 gate。
3. 将 M2 合并到 M1 gate 脚本，不单独展示。

## Consequences

1. M2 回归失败能被快速定位到 parser shadow 门槛。
2. 默认切换评估的证据链更清晰（M1 + M2 均有显式门槛）。
