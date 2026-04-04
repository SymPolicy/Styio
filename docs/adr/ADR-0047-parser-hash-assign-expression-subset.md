# ADR-0047: NewParser Hash 支持无箭头赋值表达式体

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

当前 NewParser hash 子集已覆盖 `=>` 路径，但 `# f(...) : T = expr` / `# f(...) : T := expr` 这类 legacy 已支持的无箭头表达式体仍会回退。  
这会扩大 fallback 面并降低 `#` 语法子集覆盖率。

## Decision

1. 在 hash 子集中保留 `=` / `:=` 后“可选参数列表”的逻辑。
2. 若后续 token 不是 `=>`，且已出现 `=` / `:=`，则直接解析 `expr` 作为函数体并构造 `SimpleFuncAST`。
3. 若未出现 `=` / `:=` 且也没有 `=>`，维持语法错误。
4. 新增引擎一致性回归样例：
   - `# mix(a: i32, b: i32) : i32 = a + b`。

## Alternatives

1. 继续要求 hash 子集必须走 `=>`，无箭头形态全部 fallback。
2. 无箭头分支直接复用 legacy `parse_expr`，不走子集表达式。
3. 一次性迁移 hash 所有 legacy 分支后再补这个形态。

## Consequences

1. NewParser 在 hash 函数声明上与 legacy 进一步对齐。
2. `parse_hash_tag` fallback 面继续缩小，迁移风险保持可控。
3. 回归覆盖从“子集 AST 比对”扩展到“CLI 引擎等价验证”，可更早发现行为偏差。
