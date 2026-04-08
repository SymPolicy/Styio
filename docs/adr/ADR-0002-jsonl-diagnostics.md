# ADR-0002: CLI 结构化错误输出采用 JSONL

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0002: CLI 结构化错误输出采用 JSONL.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

Styio 需要稳定的机器可读诊断格式用于未来 IDE/LSP 集成，同时保留人类可读 stderr。

## Decision

新增 `--error-format=jsonl`：

1. 默认仍为文本 stderr。
2. `jsonl` 模式每条诊断输出一行 JSON。
3. 首版稳定字段：`category`、`code`、`file`、`message`。

## Alternatives

1. 仅文本输出。
2. 单次编译输出一个大 JSON 对象。

## Consequences

1. 与流式工具兼容性更好（可逐行消费）。
2. 需要维护字段稳定性与转义规则。
