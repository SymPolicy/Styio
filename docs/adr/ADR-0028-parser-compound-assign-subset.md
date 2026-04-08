# ADR-0028: NewParser Compound Assign 子集扩展

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0028: NewParser Compound Assign 子集扩展.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E.4 前三段已覆盖 `print`、`flex bind` 与 simple `final bind`。M1 高频语句中仍有 `x += y` 等复合赋值路径由 legacy 独占，导致 new 子集覆盖不连续。

## Decision

1. 在 `parse_stmt_new_subset(...)` 增加 5 类复合赋值：
   - `+=`、`-=`、`*=`、`/=`、`%=`
2. 语句 token 白名单加入：
   - `COMPOUND_ADD`、`COMPOUND_SUB`、`COMPOUND_MUL`、`COMPOUND_DIV`、`COMPOUND_MOD`
3. 语义映射与 legacy 对齐：
   - 统一生成 `BinOpAST(Self_*_Assign, NameAST(id), expr)`
4. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnCompoundAssignSubsetSamples`
   - `StyioParserEngine.LegacyAndNewMatchOnM1CompoundAssignSample`

## Alternatives

1. 继续让 compound assign 走 legacy，不在 E.4 扩展。
2. 先迁移复杂调用/控制流，再回头处理 compound assign。
3. 直接在 E.4 一次性接管全部语句域。

## Consequences

1. new 子集在 M1 绑定语句域覆盖更完整。
2. 复合赋值路径可进入 shadow compare 流程，差异可更早暴露。
3. 仍保持非子集回退策略，不改变默认主行为。
