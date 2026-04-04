# ADR-0026: NewParser FinalBind 子集与安全回退边界

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

在 E.4 第二段完成 `flex bind` 后，`x : T := expr` 仍由 legacy 独占。为了继续扩大语句子集覆盖，需要在 new 路径接管最常见的 typed bind 形态。

同时，shadow 路由中如果子集解析器抛异常，必须保证可以无损回退 legacy，避免把异常传播为用户可见崩溃。

## Decision

1. 在 `parse_stmt_new_subset(...)` 中新增简单 typed bind 形态：
   - `NAME : NAME := expr`
   - 其中类型先限定为“简单类型名”而非复杂类型表达式。
2. 语句白名单增加 `TOK_COLON` 与 `WALRUS`，允许该子集语法进入 new 路由。
3. `parse_main_block_new_subset(...)` 改为局部 `unique_ptr` 持有中间语句，成功后再 release，避免异常路径泄漏。
4. `parse_main_block_new_shadow(...)` 在调用 new 子集解析时新增异常回退：任何异常都恢复游标并回退 legacy。
5. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnFinalBindSubsetSamples`
   - `StyioParserEngine.LegacyAndNewMatchOnM1TypedBindSample`

## Alternatives

1. 继续让 typed bind 完全走 legacy，不在 E.4 扩展。
2. 一次性支持复杂类型语法（数组/边界类型等）后再合并。
3. 让 new 解析异常直接上抛，不提供自动回退。

## Consequences

1. new 路径语句覆盖继续扩展到 typed bind 的高频子集。
2. 对复杂类型仍保持保守策略，不会因未实现分支破坏稳定性。
3. shadow 路由具备异常回退能力，减少中间状态对主流程的影响。
