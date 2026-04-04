# ADR-0048: NewParser Hash 支持无赋值直接箭头体

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

在 `ADR-0047` 后，NewParser 已支持 `=` / `:=` 的无箭头表达式体。  
但 legacy 还支持 `# f : T => expr` 与 `# f => expr`，这两类无赋值直接箭头体仍会回退。

## Decision

1. hash 子集在 `=` / `:=` 分支之外，允许直接命中 `=>`（不再要求已有参数列表）。
2. `=>` 后继续复用现有策略：
   - block 体走 `parse_block_with_forward`；
   - 非 block 体走 `parse_stmt_new_subset`。
3. 新增回归样例：
   - `# const42 : i32 => 42`
   - `# ping => 1`

## Alternatives

1. 维持旧限制：只有 `# f(params) => ...` 才走子集，其他直接 fallback。
2. 把无赋值箭头体放到后续“大一刀”统一处理。
3. 只支持带返回类型 `# f : T => ...`，不支持 `# f => ...`。

## Consequences

1. hash 子集与 legacy 在常见函数声明形态上进一步贴齐。
2. fallback 面再缩小一段，E.7 迁移进度更线性。
3. 新增 CLI 引擎一致性用例，保证行为差异可被快速发现。
