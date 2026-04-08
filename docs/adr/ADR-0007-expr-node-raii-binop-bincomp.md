# ADR-0007: Expr 节点 RAII 第一段（BinOp/BinComp）

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0007: Expr 节点 RAII 第一段（BinOp/BinComp）.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

C.3 目标是将 AST 节点从裸指针逐步迁移到 RAII。  
`BinOpAST` 和 `BinCompAST` 是表达式主干节点，历史实现仅持有裸指针，不负责子节点释放，导致生命周期管理依赖外部约定且容易泄漏。

## Decision

1. `BinOpAST` 改为内部以 `std::unique_ptr` 持有：
   - `TypeAST` 类型对象；
   - 左右子表达式。
2. `BinCompAST` 改为内部以 `std::unique_ptr` 持有左右子表达式。
3. 对外保留既有 raw-pointer 访问形态（`LHS/RHS/getLHS/getRHS`），以降低 parser/typeinfer/codegen 级联改动风险。
4. 新增安全回归测试，验证 `delete BinOpAST/BinCompAST` 时会触发子表达式析构。

## Alternatives

1. 一次性把所有 Expr/Stmt/Decl 节点同时迁移到 `unique_ptr`。
2. 保持裸指针，继续依赖 `CompilationSession` 末端统一回收。
3. 直接把 AST 对外接口全部改成 `std::unique_ptr` 传递。

## Consequences

1. 在不破坏现有调用点的前提下，先把最核心表达式节点纳入 RAII。
2. 可为后续 C.3/C.4 分段迁移提供可复用模式（内部 owning，外部兼容视图）。
3. 仍存在未迁移节点族，需继续按微里程碑推进。
