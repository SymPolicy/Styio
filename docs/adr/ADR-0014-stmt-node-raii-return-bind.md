# ADR-0014: Stmt/Decl RAII 第一段（Return/FlexBind/FinalBind）

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

进入 C.4 阶段后，部分语句节点仍持有裸指针子节点：

1. `ReturnAST::Expr`
2. `FlexBindAST::{variable, value}`
3. `FinalBindAST::{var_name, val_expr}`

这些节点在函数体与绑定语句中高频出现，所有权依赖外部协议，不利于中断恢复后的边界一致性。

## Decision

1. `ReturnAST` 改为内部 `unique_ptr<StyioAST>` 持有返回表达式。
2. `FlexBindAST` 与 `FinalBindAST` 改为内部 `unique_ptr` 持有变量节点和 RHS 表达式。
3. 对外 getter 行为保持不变（raw pointer 读取），避免影响 typeinfer/codegen 调用面。
4. 在 `security` 新增 `Return/FlexBind/FinalBind` 析构回归测试，锁定所有权契约。

## Alternatives

1. 继续依赖 tracked cleanup，不在语句节点内部收口。
2. 一次性迁移全部 Stmt/Decl 节点，扩大单次改动面。

## Consequences

1. C.4 首批语句节点所有权边界收敛。
2. 绑定与返回路径的生命周期不再依赖外部 delete 协议。
3. 后续可继续按同样模式迁移 `Var/Param/ReadFile/...` 等节点族。
