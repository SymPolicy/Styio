# ADR-0081: TypeTuple/CheckEqual RAII 所有权收口

- Status: Accepted
- Date: 2026-04-06

## Context

C.4/C.5 多轮迁移后，`AST.hpp` 中仍有两类容器节点保留裸指针集合：

1. `TypeTupleAST::type_list`（函数返回类型元组）；
2. `CheckEqualAST::right_values`（`?= ...` 比较分支右值集合）。

它们在删除根节点时不会释放子节点，导致 ownership 回归存在遗漏。

## Decision

1. `TypeTupleAST` 引入 `type_owners_`（`std::vector<std::unique_ptr<TypeAST>>`），并通过 `adopt_type_list(...)` 统一接管所有权。
2. `CheckEqualAST` 引入 `right_value_owners_`（`std::vector<std::unique_ptr<StyioAST>>`），并通过 `adopt_right_values(...)` 统一接管所有权。
3. 对外保留 `type_list` / `right_values` 作为 view，避免影响既有 `Parser/ToString/Analyzer` 调用点。
4. 补充 safety 回归：
   - `StyioSecurityAstOwnership.TypeTupleOwnsTypeNodes`
   - `StyioSecurityAstOwnership.CheckEqualOwnsRightValueExprs`

## Alternatives

1. 继续保留裸指针并依赖全局 tracked node 清理：
   - 拒绝。该策略无法表达局部 ownership 边界，且会弱化析构可验证性。
2. 直接把对外接口改为 `std::vector<std::unique_ptr<...>>`：
   - 拒绝。会扩散到大量调用点，不符合 1-3 天 checkpoint 的变更半径。

## Consequences

1. `TypeTupleAST` 和 `CheckEqualAST` 删除时会级联释放子节点，析构行为可测试。
2. AST 对外读取接口保持兼容，当前流水线无需额外迁移。
3. C.4 owner 收口覆盖面进一步提升，可继续向剩余节点做同模式推进。
