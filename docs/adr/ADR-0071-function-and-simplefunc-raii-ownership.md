# ADR-0071: `FunctionAST` / `SimpleFuncAST` 的 RAII 所有权收口

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

在 C.3/C.4 的 AST 内存安全推进中，`FunctionAST` 与 `SimpleFuncAST` 仍保留“全裸指针聚合”：

1. `func_name`、`params`、`ret_type`、`func_body` / `ret_expr` 仅保存原始指针；
2. 删除函数节点时不会递归释放其子树；
3. 该节点位于语义分析与代码生成的高频主路径，泄漏风险随函数数量线性放大。

红灯回归可稳定复现：

- `StyioSecurityAstOwnership.FunctionOwnsNameParamsRetTypeAndBody`
- `StyioSecurityAstOwnership.SimpleFuncOwnsNameParamsRetTypeAndReturnExpr`

修复前两条用例析构计数均为 `0`。

## Decision

1. 为 `FunctionAST` 引入内部 owner：
   - `std::unique_ptr<NameAST> func_name_owner_`
   - `std::vector<std::unique_ptr<ParamAST>> params_owner_`
   - `std::unique_ptr<TypeAST> ret_type_owner_`
   - `std::unique_ptr<TypeTupleAST> ret_type_tuple_owner_`
   - `std::unique_ptr<StyioAST> func_body_owner_`
2. 为 `SimpleFuncAST` 引入同构 owner：
   - `func_name_owner_` / `params_owner_` / `ret_type_owner_` / `ret_type_tuple_owner_` / `ret_expr_owner_`
3. 保持对外兼容：
   - 继续暴露 `func_name`、`params`、`ret_type`、`func_body` / `ret_expr` 作为 raw view；
   - 构造阶段完成所有权接管，调用侧无需改签名。
4. `ret_type` 仍保持 `std::variant<TypeAST*, TypeTupleAST*>`，只把生命周期迁入 owner，避免大面积接口扰动。

## Alternatives

1. 保持裸指针并依赖全局回收（释放边界不明确，且难以定位泄漏来源）。
2. 在 Parser/Analyzer 手动逐层 delete（维护成本高，易重复释放或漏释放）。
3. 一次性改造所有函数/循环/模块节点（跨度过大，不符合 checkpoint 小步合并策略）。

## Consequences

1. 函数定义节点释放语义稳定，不再依赖外部调用链“记得清理”。
2. 回归测试新增两条 ownership 断言，作为中断恢复后的红灯书签。
3. 对外 API 兼容，变更局限在节点内部生命周期管理，适合 1-3 天合并节奏。
