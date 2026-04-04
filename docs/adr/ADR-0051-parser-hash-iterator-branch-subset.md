# ADR-0051: NewParser Hash 子集接管 `>>` Iterator 分支

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

在 `ADR-0050` 后，`#` 语法中的 `?=` 分支已进入 NewParser 子集。  
仍未接管的高频分支之一是 `# f(x) >> ...`（iterator），当前完全依赖 fallback。

## Decision

1. 在 `parse_hash_stmt_new_subset` 中新增 `ITERATOR` 分支接管：
   - 复用 legacy `parse_iterator_with_forward(context, params[0])`。
2. 与 legacy 对齐约束：
   - 当参数数目不为 1 时抛出同类语法错误（iterator 不能应用于多个对象）。
3. 扩展 stmt token gate，纳入 `ITERATOR`。
4. 新增 parser 等价回归：
   - `# iter_only(x) >> (n) ?= 2 => >_(n)`。

## Alternatives

1. 保持 iterator 分支继续 fallback。
2. 一次性重写 iterator 语法，不复用 legacy parser。
3. 仅放宽 token gate，不接管语义分支。

## Consequences

1. hash 子集进一步收口，fallback 面继续缩小。
2. 迭代器语义仍由 legacy 实现承载，迁移风险较低。
3. CLI 层该语法存在既有运行时崩溃样例，本次不将崩溃固化为通过条件，仅在 parser 等价层回归。
