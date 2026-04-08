# ADR-0039: NewParser 接管函数定义入口（复用 legacy 语义体）

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0039: NewParser 接管函数定义入口（复用 legacy 语义体）.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E.7 需要把 parser 覆盖从表达式/绑定扩展到函数相关语法（M2）。一次性重写 `#` 函数语法风险较高，且会耦合参数、返回类型、块体语义。

## Decision

1. NewParser 语句子集接管 `TOK_HASH` 入口。
2. 具体函数语义体继续复用 legacy `parse_hash_tag(context)`。
3. 为通过 token 门，补齐函数定义相关 token：
   - `TOK_HASH`
   - `ARROW_DOUBLE_RIGHT`
   - `TOK_LCURBRAC` / `TOK_RCURBRAC`
   - `EXTRACTOR`
4. 新增回归：
   - `StyioSecurityNewParserStmt.SubsetTokenGateIncludesFunctionDefTokens`
   - `StyioSecurityNewParserStmt.MatchesLegacyOnFunctionDefSubsetSamples`
   - `StyioParserEngine.LegacyAndNewMatchOnM2SimpleFuncSample`
   - `StyioParserEngine.ShadowCompareAcceptsM2CoreSuite`

## Alternatives

1. 函数定义继续全部回退 legacy，不进入 NewParser 子集。
2. 直接在 NewParser 重写完整 `parse_hash_tag`。
3. 只加 token 白名单，不接管语句入口。

## Consequences

1. M2 主路径可纳入 shadow compare，覆盖面提升。
2. 语义风险受控（核心实现仍由 legacy 提供）。
3. 后续可按微里程碑把 `parse_hash_tag` 的子路径逐步迁到 NewParser。
