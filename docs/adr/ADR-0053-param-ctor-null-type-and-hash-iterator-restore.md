# ADR-0053: ParamAST 构造对齐与 Hash Iterator 恢复

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

在临时 crash guard 放开后，`# iter_only(x) >> (n) => >_(n)` 仍会在 legacy/new 路径段错误（exit 139）。  
通过 `lldb` 回溯定位到 `ToStyioIR.cpp:param_to_sgarg`，根因是未标注类型的 `ParamAST` 出现 `var_type == nullptr`，在函数参数下放到 SG 层时被解引用触发崩溃。

## Decision

1. 修复 `ParamAST` 构造函数：
   - 统一将 `var_name` / `var_type` / `val_init` 与 `VarAST` 基类字段对齐；
   - 保证“未显式类型参数”也持有默认 `TypeAST`（Undefined）。
2. 恢复 hash iterator 定义语法：
   - legacy `parse_hash_tag` 与 NewParser `parse_hash_stmt_new_subset` 重新接管 `ITERATOR` 分支；
   - 保留单参数约束（多参数时报语法错误）。
3. 回归策略从“稳定拒绝”切回“稳定可执行/可比较”：
   - `StyioParserEngine.LegacyAndNewMatchOnHashIteratorDefinition`
   - `StyioSecurityNewParserStmt.MatchesLegacyOnFunctionDefSubsetSamples`（含 hash iterator 定义样例）。

## Alternatives

1. 继续维持 ADR-0052 的 parser 层禁用 guard，不恢复语法。
2. 仅在 hash iterator 分支局部绕过 `ParamAST`，不修复 `ParamAST` 构造本体。
3. 保持崩溃现状，等待后续大版本重构统一处理。

## Consequences

1. 未标注类型参数的函数下放不再因空类型指针崩溃。
2. hash iterator 定义路径从“临时禁用”恢复为“可解析且双引擎行为一致”。
3. ADR-0052 仍保留为“风险收敛历史记录”，当前行为以 ADR-0053 为准。
