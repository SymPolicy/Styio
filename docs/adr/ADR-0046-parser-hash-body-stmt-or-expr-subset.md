# ADR-0046: NewParser Hash 非 block 函数体采用 stmt-or-expr 子集

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0046: NewParser Hash 非 block 函数体采用 stmt-or-expr 子集.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

`ADR-0045` 已对齐 hash 签名与返回类型子集，但 `# ... => body` 的非 block 分支仍只走 `parse_expr_new_subset`。  
这会把 `=> >_(...)` 这类“语句型函数体”强制回退 legacy，覆盖面仍偏窄。

## Decision

1. hash 非 block 函数体从 `parse_expr_new_subset` 切换为 `parse_stmt_new_subset`。
2. 保持外层 rollback/fallback 机制不变：若子集语句解析失败，仍由上层恢复游标并回退 `parse_hash_tag`。
3. 新增回归样例冻结行为：
   - `# alert := () => >_("ALERT")`。

## Alternatives

1. 继续仅支持表达式函数体，维持 fallback。
2. 在 hash 体内单独实现 print 特判分支。
3. 直接调用 legacy `parse_stmt_or_expr`。

## Consequences

1. NewParser 可原生覆盖更多 `#` 函数体形态（尤其 print 语句体）。
2. 复用已有语句子集逻辑，减少重复解析分支。
3. 风险边界仍受 fallback 保护，不扩大失败影响面。
