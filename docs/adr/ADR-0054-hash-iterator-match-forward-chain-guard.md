# ADR-0054: Hash Iterator `?=` Forward Chain 稳定拒绝

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0054: Hash Iterator `?=` Forward Chain 稳定拒绝.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

在恢复 hash iterator 定义后，`# f(x) >> (n) ?= 2 => ...` 这类 `?=` + `=>` 组合语法会被解析，但运行期语义会被静默吞掉（表现为空输出），且与预期不一致。  
核心原因是 iterator forward 链路当前仅支持单子句，`CheckEqual/Cases` 作为首子句缺少专用 runtime 语义。

## Decision

1. 在 `parse_iterator_with_forward` 增加 guard：
   - 若首个 forward 子句是 `CheckEqualAST` 或 `CasesAST`，直接抛 `StyioNotImplemented`；
   - 若 forward 子句数量大于 1，直接抛 `StyioNotImplemented`。
2. 保持错误通道稳定：
   - CLI 统一映射为 `ParseError`（退出码 `3`）。
3. 新增回归：
   - `StyioParserEngine.HashIteratorMatchForwardChainReturnsParseError`。

## Alternatives

1. 保持现状，允许语法“成功”但语义静默错误。
2. 立即实现完整的 iterator `?=` forward runtime 语义。
3. 在 tokenizer/grammar 层整体禁掉 `?=` 出现在 iterator forward 中。

## Consequences

1. 对用户侧行为从“静默误行为”收敛为“稳定、可诊断的错误”。
2. 简单 hash iterator 定义（`>> (n) => ...`）仍保持可用。
3. 后续可单独拆分语义实现切片，在不破坏主干稳定性的前提下恢复该组合语法。
