# ADR-0024: NewParser 语句子集第一段（print + 表达式语句）

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0024: NewParser 语句子集第一段（print + 表达式语句）.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

在 E.3 中，`new` 路径只覆盖了表达式子集。E.4 需要开始迁移语句域，但不能一次性切入完整 `parse_stmt_or_expr`，否则变更面过大且不利于 checkpoint 合并。

当前最小可落地语句域是：`print` 语句与表达式语句；它们可直接复用 E.3 的表达式子集能力，并且对主流程风险最低。

## Decision

1. 在 `NewParserExpr` 模块中扩展语句子集能力：
   - `parse_main_block_new_subset(...)`
   - `styio_new_parser_is_stmt_subset_start(...)`
   - `styio_new_parser_is_stmt_subset_token(...)`
2. 首段语句子集仅接管：
   - `>_(...)` 打印语句（参数由 `parse_expr_new_subset(...)` 解析）；
   - 纯表达式语句。
3. `parse_main_block_new_shadow(...)` 的路由门从“表达式 token 白名单”升级为“语句 token 白名单”；任何超集语法继续回退 legacy。
4. 新增回归测试 `StyioSecurityNewParserStmt.MatchesLegacyOnPrintSubsetSamples`，验证 print 子集在多样本上与 legacy AST 表示一致。

## Alternatives

1. 继续只保留 E.3 表达式子集，不推进语句域。
2. 一次性迁移完整 `parse_stmt_or_expr` 语法。
3. 在 E.4 中优先迁移 bind/控制流等更复杂语句。

## Consequences

1. `new` 路径语句覆盖率从 0 提升到最小可用子集，且默认行为仍可回退到 legacy。
2. E.5 影子比对可以在 print/表达式语句域先建立稳定基线，再扩展到 bind/控制流。
3. 后续语句迁移需继续采用“白名单扩容 + 非子集回退”的策略，避免大范围语义漂移。
