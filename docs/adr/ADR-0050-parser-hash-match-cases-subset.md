# ADR-0050: NewParser Hash 子集接管 `?=` Match Cases 分支

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0050: NewParser Hash 子集接管 `?=` Match Cases 分支.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

`ADR-0047/0048` 后，NewParser 已覆盖多数 hash 函数定义形态，但 `# ... ?= ...` 仍完全依赖 `parse_hash_tag` fallback。  
这会让 `#` 语法迁移留下明显“黑洞”分支，影响 E.7 收口节奏。

## Decision

1. 在 `parse_hash_stmt_new_subset` 中新增 `MATCH` 分支：
   - `# f(...) ?={ ... }` 走 `parse_cases_only(context)`；
   - `# f(...) ?= expr[, expr...]` 走 `CheckEqualAST::Create(...)`。
2. `styio_new_parser_is_stmt_subset_token` 增补 `MATCH` 与 `TOK_UNDLINE`，允许 hash cases 进入 new-shadow 预检集合。
3. 保持 fallback 机制不变：
   - 子集分支抛错时，仍恢复游标并回退 legacy。
4. 新增回归：
   - security：`# parity(n: i32) ?={ ... }`
   - parser engine：legacy/new CLI 输出一致性。

## Alternatives

1. 继续将 `?=` 分支完全留给 legacy fallback。
2. 仅放开 token gate，不接管语义分支。
3. 一次性迁移 iterator/forward/match 全部 hash 分支。

## Consequences

1. hash 子集覆盖进一步扩展，fallback 面继续缩小。
2. M3 类型 token（`MATCH`/`_`）进入子集 gate，new parser 对非子集输入会更常触发“尝试后回退”，但行为仍安全一致。
3. 后续可把 `iterator (>>)` 作为下一独立切片继续迁移。
