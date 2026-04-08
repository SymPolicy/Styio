# ADR-0034: 重构模板标准化 Shadow Gate 模式

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

本轮 parser 双轨重构已经形成稳定实践：显式 shadow compare、差异工件落盘、CI 显式 gate、失败自动上传工件。若只停留在本轮历史记录，后续维护很容易重复踩坑。

## Decision

1. 在 `docs/assets/templates/REFACTOR-WORKFLOW-TEMPLATE.md` 增加 “双轨重构附加模板（Shadow Gate）”。
2. 将该模式作为后续高风险双轨改造的默认附加清单：
   - 显式引擎开关
   - 显式 shadow compare 开关
   - artifact dir 参数
   - CI 显式 gate
   - 失败工件上传

## Alternatives

1. 仅在 `docs/history/` 保留本次经验，不写通用模板。
2. 在单个 ADR 里记录，不改模板文档。
3. 等到更多模块复用后再统一总结。

## Consequences

1. 后续重构可直接套模板，减少流程设计成本。
2. 中断恢复和回放能力成为默认工程习惯，而非个人经验。
3. 文档维护量略增，但可复用价值更高。
