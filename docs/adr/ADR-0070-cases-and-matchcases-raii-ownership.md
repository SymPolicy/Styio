# ADR-0070: `CasesAST` / `MatchCasesAST` 的 RAII 所有权收口

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

C.3/C.4 收口过程中，`CasesAST` 与 `MatchCasesAST` 仍使用裸指针聚合：

1. `CasesAST` 仅保存 `case_list` 与 `case_default` 的原始指针；
2. `MatchCasesAST` 仅保存 `Value` 与 `Cases` 的原始指针；
3. 直接 `delete` 根节点时，不会递归释放子表达式，存在确定性泄漏风险。

红灯回归已复现：

- `StyioSecurityAstOwnership.CasesOwnsCasePairsAndDefault`
- `StyioSecurityAstOwnership.MatchCasesOwnsScrutineeAndCases`

修复前两条用例的析构计数均为 `0`。

## Decision

1. `CasesAST` 引入内部 owner：
   - `std::vector<std::pair<std::unique_ptr<StyioAST>, std::unique_ptr<StyioAST>>> case_owners_`
   - `std::unique_ptr<StyioAST> default_owner_`
2. 对外仍保留兼容视图：
   - `case_list` 与 `case_default` 继续暴露 `StyioAST*`，减少调用侧改动。
3. `MatchCasesAST` 引入内部 owner：
   - `std::unique_ptr<StyioAST> value_owner_`
   - `std::unique_ptr<CasesAST> cases_owner_`
4. 构造时完成所有权接管，getter/现有行为保持不变。

## Alternatives

1. 保持裸指针并依赖全局 `destroy_all_tracked_nodes`（释放边界不清晰，易重复/遗漏）。
2. 在调用点手动 `delete` 所有分支节点（易漏，且增加调用栈复杂度）。
3. 一次性改造所有控制流节点（跨度过大，不符合 checkpoint 小步合并原则）。

## Consequences

1. `CasesAST` / `MatchCasesAST` 子树释放语义稳定，不再依赖外部“记得清理”。
2. 与 C.3 微里程碑一致：通过小范围节点族改造持续降低泄漏面。
3. 兼容性成本低：公共接口保持原样，仅内部所有权模型升级。
