# ADR-0001: Checkpoint 微里程碑工作流

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

Styio 的底层重构跨度长、模块耦合高，长分支容易出现 branch rot 与上下文丢失。

## Decision

采用 checkpoint 微里程碑执行模型：

1. 单分支存活不超过 72 小时。
2. 每个 checkpoint 必须可编译/可测试/可回滚。
3. 每次 checkpoint 必须同步：代码、测试、文档、ADR、history 恢复指引。

## Alternatives

1. 保持周级大分支后一次性合并。
2. 仅靠口头/聊天记录恢复上下文。

## Consequences

1. 合并频率提升，单次 PR 体积下降。
2. 文档维护开销上升，但中断恢复成本显著下降。
