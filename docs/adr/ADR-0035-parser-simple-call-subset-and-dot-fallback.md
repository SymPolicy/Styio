# ADR-0035: NewParser 接管简单调用子集并保留 dot-call 回退

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0035: NewParser 接管简单调用子集并保留 dot-call 回退.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

旧实现对 `name(` 序列做了硬编码预回退，导致所有调用表达式都无法进入 NewParser，即使是最简单的 `foo(1, 2)` 也只能走 legacy。这成为 E.7 清退前的主要阻塞点之一。

## Decision

1. NewParser 表达式子集新增简单调用解析：
   - 语法：`name(args...)`
   - 位置：可作为表达式语句、print 参数、赋值 RHS。
2. 移除 `name(` 的预扫描硬回退，改为：
   - 先按现有 token 子集门尝试 NewParser；
   - 解析失败仍走既有异常回退到 legacy。
3. 保守边界保持不变：
   - `dot-call`（如 `foo.bar(1)`）仍不在子集，由 token 门/异常路径回退 legacy。
4. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnSimpleCallSubsetSamples`
   - `StyioSecurityNewParserShadow.FallsBackOnDotCallSequence`

## Alternatives

1. 保持全部调用都回退 legacy，暂不接管。
2. 一次性接管包含 dot-call 在内的完整调用链。
3. 继续保留 `name(` 预回退，再单独开关局部放开。

## Consequences

1. NewParser 覆盖面扩大到简单调用表达式，减少 legacy 独占路径。
2. 保持保守回退策略，不引入默认行为变更。
3. 为后续迁移复杂调用链（dot/chain call）提供独立下一刀切入点。
