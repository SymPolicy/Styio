# ADR-0013: Expr 节点 RAII 第七段（Tuple/List/Set）

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0013: Expr 节点 RAII 第七段（Tuple/List/Set）.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

集合表达式节点仍在内部保存裸指针元素：

1. `TupleAST::elements`
2. `ListAST::elements_`
3. `SetAST::elements_`

同时 `TupleAST/ListAST` 的 `consistent_type` 也通过裸指针初始化，析构边界依赖外部约定。

## Decision

1. `TupleAST/ListAST/SetAST` 引入 `vector<unique_ptr<StyioAST>>` 持有元素，继续对外提供现有 raw 视图 getter。
2. `TupleAST/ListAST` 将 `consistent_type` 改为内部 `unique_ptr<TypeAST>` 持有，并保留 raw pointer 访问兼容。
3. `VarTupleAST` 继承路径保持不变，底层元素所有权由 `TupleAST` 基类统一接管。
4. 在 `security` 新增 `Tuple/List/Set/VarTuple` 析构回归，锁定集合节点生命周期契约。

## Alternatives

1. 延后到 C.4 再统一处理集合节点。
2. 保留裸指针，只依赖 tracked 节点登记清理。

## Consequences

1. C.3 的 Expr owning 覆盖到集合核心节点。
2. `VarTupleAST` 与 `TupleAST` 生命周期边界一致，减少二义性。
3. 后续可将焦点从 Expr 迁移到 C.4 的 Stmt/Decl 节点族。
