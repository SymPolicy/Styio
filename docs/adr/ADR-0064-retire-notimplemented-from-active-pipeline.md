# ADR-0064: 活跃编译流水线移除 `Styio.NotImplemented` 错误类别

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0064: 活跃编译流水线移除 `Styio.NotImplemented` 错误类别.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

F.2 收敛前，活跃路径（Parser / Analyzer / CodeGen）中仍存在 `StyioNotImplemented` 抛错。  
虽然顶层分类有时仍显示为 `ParseError` 或 `TypeError`，但 message 前缀会泄漏 `Styio.NotImplemented`，造成：

1. 同类用户输入边界在错误文案上不一致；
2. 机器侧（JSONL + IDE）需要额外适配“实现未完成”前缀；
3. 高风险路径在阶段切换时容易漂移成 `RuntimeError`。

## Decision

1. 活跃流水线中不再抛 `StyioNotImplemented`：
   - `Parser.cpp` 统一改为 `StyioParseError`；
   - `ToStyioIR.cpp` 统一改为 `StyioTypeError`；
   - `CodeGenG.cpp` 已在 F.2.0/F.2.1 改为 `StyioTypeError`。
2. `StyioNotImplemented` 仅保留给 `src/Deprecated/` 历史路径，不再用于当前执行链路。
3. 补诊断回归，显式断言关键路径不再出现 `Styio.NotImplemented` 文本。

## Alternatives

1. 保留 `StyioNotImplemented`，只依赖顶层 category 做分流。
2. 在 `main` 做统一 message 重写（隐藏原异常类型）。
3. 单独新增“UnsupportedError”并逐步迁移。

## Consequences

1. 当前用户可见错误语义更稳定：语法边界 -> `ParseError`，语义/类型边界 -> `TypeError`。
2. JSONL 诊断 message 与 category 一致，不再出现跨类别前缀漂移。
3. 未来新增“不支持”分支必须在 Parser/Analyzer/CodeGen 选择准确错误类型，不能回退到 `StyioNotImplemented`。
