# ADR-0025: NewParser 语句子集第二段（FlexBind + 调用序列回退）

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E.4 第一段已接入 `print` + 表达式语句。下一步需要覆盖最常见的赋值语句 `x = expr`，否则 new 路径对 M1 的语句覆盖仍不完整。

同时，当前表达式子集不支持函数调用语法（如 `foo(1)`）。若仅按 token 白名单路由，`name(` 序列会被误判为可接管，导致 new 路径出现语义偏差。

## Decision

1. 在 `parse_stmt_new_subset(...)` 中新增 `NAME '=' expr` 分支，生成 `FlexBindAST`。
2. 语句 token 白名单增加 `TOK_EQUAL`，允许 bind 子集通过路由门。
3. 在 shadow 路由前增加序列级保守检查：命中 `name(` 时直接回退 legacy（new 仍不接管调用语法）。
4. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnFlexBindSubsetSamples`
   - `StyioSecurityNewParserShadow.FallsBackOnCallExpressionSequence`

## Alternatives

1. 继续只支持 print，不迁移任何绑定语句。
2. 直接在 E.4 同步迁移函数调用语法。
3. 不做序列级回退，仅依赖 token 白名单。

## Consequences

1. new 路径可覆盖最常见的 M1 绑定语句，E.4 语句覆盖继续扩张。
2. `foo(1)` 等调用模式在 new 未实现前不会被误接管，降低语义偏差风险。
3. 后续若实现调用语法，需要显式移除该回退门并补对应对齐测试。
