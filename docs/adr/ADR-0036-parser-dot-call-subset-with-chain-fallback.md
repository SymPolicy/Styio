# ADR-0036: NewParser 接管 Dot-Call 子集并保留 Chain 回退

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

上一刀已接管 `name(args...)` 简单调用，但 `foo.bar(...)` 仍落在 legacy 路径。与此同时，legacy 对更复杂的链式调用（如 `foo.bar(1).baz(2)`）本身尚不支持，直接全量接管会扩大风险面。

## Decision

1. NewParser 表达式子集新增 dot-call：
   - 支持 `name.method(args...)`
2. token 子集门新增 `TOK_DOT`，允许该语法进入 new 路径。
3. 调用识别采用“同一行”边界：
   - 使用 `skip_spaces_no_linebreak()`，不跨行把 `name` 与 `(` / `.` 拼接成调用。
4. 复杂 dot-chain 继续保守回退：
   - 对 legacy/new 都不支持的 `foo.bar(1).baz(2)`，保持一致失败行为。
5. 回归补齐：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnDotCallSubsetSamples`
   - `StyioSecurityNewParserShadow.FallsBackOnDotChainSequence`

## Alternatives

1. dot-call 继续全部走 legacy。
2. 一次性接管完整 chain-call 语义。
3. 放开 `TOK_DOT` 但不加回归。

## Consequences

1. NewParser 覆盖面扩大到常见方法调用形式。
2. 保持与 legacy 在高风险 chain-call 上的一致失败边界。
3. 为后续 chain-call 迁移保留独立小步切入点。
