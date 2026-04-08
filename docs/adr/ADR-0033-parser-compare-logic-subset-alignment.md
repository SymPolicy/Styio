# ADR-0033: NewParser 比较/逻辑子集接管与 AST 对齐

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0033: NewParser 比较/逻辑子集接管与 AST 对齐.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

NewParser 表达式子集原本已具备比较/逻辑运算的优先级框架，但语句白名单未放开这些 token，导致相关输入长期回退 legacy。放开白名单后又暴露 AST 形状差异：

1. 比较运算被构造成 `BinOpAST(Undefined)`；
2. 逻辑运算被构造成 `BinOpAST(Undefined)`；
3. legacy 语义分别使用 `BinCompAST` 与 `CondAST`。

## Decision

1. 扩充表达式子集 token 白名单，纳入比较/逻辑运算：
   - `BINOP_GT/GE/LT/LE/EQ/NE`
   - `TOK_RANGBRAC/TOK_LANGBRAC`
   - `LOGIC_AND/LOGIC_OR`
2. 在 NewParser 构 AST 时对齐 legacy 节点类型：
   - 比较运算 -> `BinCompAST`
   - 逻辑运算 -> `CondAST`
   - 算术运算继续使用 `BinOpAST`
3. 新增安全回归：
   - `StyioSecurityNewParserExpr.SubsetTokenGateIncludesCompareAndLogic`
   - `StyioSecurityNewParserStmt.MatchesLegacyOnCompareAndLogicSubsetSamples`

## Alternatives

1. 比较/逻辑继续走 legacy，不在当前阶段接管。
2. 在 NewParser 内保留 `BinOpAST`，后续统一 AST 正规化。
3. 先扩 token 白名单，暂不增加回归覆盖。

## Consequences

1. NewParser 在 M1 比较/逻辑表达式上的覆盖提升，减少不必要回退。
2. shadow compare 差异面缩小，E.6 gate 更稳定。
3. AST 结构兼容性更强，为后续默认切换降低风险。
