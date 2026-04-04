# ADR-0023: NewParser 表达式子集与保守 Shadow 路由

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E.3 目标是在不改变默认行为（`legacy` 仍为默认引擎）的前提下，让 `new` 路径先接管一段可控的表达式解析能力。

风险点有两个：

1. legacy 表达式实现存在历史行为（结合性/一元负号优先级等），若子集定义过宽，容易在 shadow 路由中引入语义漂移。
2. 若 `new` 子集在尾部语法上误接管，会放大双轨分歧并增加回归成本。

## Decision

1. 新增 `src/StyioParser/NewParserExpr.hpp/.cpp`，提供 `parse_expr_new_subset(...)` 与子集 token 判定函数。
2. `parse_main_block_new_shadow(...)` 仅在“全 token 落入子集 + 起始 token 合法”时走 new；任一条件不满足立即回退 legacy。
3. 子集先聚焦算术表达式域（字面量/名称/括号与 `+ - * / % **`），显式不覆盖关系与逻辑运算（这些仍走 legacy）。
4. 为减少与 legacy 的结构差异，子集解析中保持右结合链路，并让一元 `-` 捕获后续表达式（与现有 legacy 行为对齐）。
5. 安全回归新增：`StyioSecurityNewParserExpr.MatchesLegacyOnSubsetSamples` 与 `StyioSecurityNewParserExpr.RejectsNonSubsetStatementToken`。

## Alternatives

1. 直接让 `new` 处理完整表达式（含比较/逻辑）后再落地。
2. `new` 路径不做语义对齐，先接受 AST 结构差异，后续统一修复。
3. 继续仅保留 E.1/E.2 空壳，不在 E.3 引入任何真实解析能力。

## Consequences

1. E.3 可在 1-3 天 Checkpoint 内独立合并，且默认用户行为不变。
2. shadow 路由具备“先判定、后接管、失败回退”的安全边界，降低线上风险。
3. E.4/E.5 可在此基础上逐语法域扩展，并把关系/逻辑表达式作为下一批显式迁移项。
