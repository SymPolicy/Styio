# ADR-0088: New Parser 接管表达式 Postfix `?=` Match-Cases 子集

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0088: New Parser 接管表达式 Postfix `?=` Match-Cases 子集.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0087` 之后，`new` parser 已能原生处理 block/control 主体，但表达式 postfix `?={...}` 仍只存在于 legacy `parse_expr_postfix(...)`。

这意味着：

1. `label = x % 2 ?= { ... }` 这类 M3 高路径仍会让 `parse_main_block_new_subset(...)` 在表达式阶段失配；
2. `# fact := (...) => { n ?= { ... } }` 这类 block body 虽已进入 new block 子集，但遇到 `?=` 时仍会退回 legacy；
3. 默认 `new` 主路径下，M3 `match-cases` 相关输入覆盖仍不连续。

## Decision

1. 在 `NewParserExpr.cpp` 的表达式子集上新增完整表达式级 postfix 处理：
   - 仅接管 `?={...}`；
   - 复用 legacy `parse_cases_only(context)` 解析 cases body。
2. 保持切片边界明确：
   - 本次不接管 iterator / redirect / forward 等其它 postfix；
   - cases body 内部继续沿用既有 legacy parser。
3. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnMatchCasesSubsetSamples`
   - `StyioParserEngine.LegacyAndNewMatchOnM3MatchExprSample`

## Alternatives

1. 保持 `?=` 完全依赖 legacy：
   - 拒绝。会导致 M3 主路径长期停留在 fallback，E.7 无法继续线性推进。
2. 一次性重写 `cases` body 与全部 postfix：
   - 拒绝。范围过大，不符合 checkpoint 粒度要求。

## Consequences

1. `new` parser 对 M3 `match-cases` 输入的接管范围向前推进一段，top-level 与 block 内常见 `?=` 都可由 new subset 主导。
2. 当前仍保留明确边界：
   - `cases` body 继续复用 legacy；
   - 其它 postfix（iterator / redirect 等）不在本次切片内。
3. 中断恢复时可直接通过 M3 CLI 对比回归与 security 子集回归校验该边界。
