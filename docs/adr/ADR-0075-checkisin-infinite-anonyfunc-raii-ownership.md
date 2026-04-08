# ADR-0075: `CheckIsinAST` / `InfiniteAST` / `AnonyFuncAST` 的 RAII 所有权收口

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0075: `CheckIsinAST` / `InfiniteAST` / `AnonyFuncAST` 的 RAII 所有权收口.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

在 C.5 容器节点收口后，仍有一组小型节点保留裸指针字段：

1. `CheckIsinAST::Iterable`
2. `InfiniteAST::Start` / `IncEl`
3. `AnonyFuncAST::Args` / `ThenExpr`

这些节点位于表达式与函数语法链路上，若删除根节点不递归释放子树，会形成稳定泄漏。

红灯回归：

- `StyioSecurityAstOwnership.CheckIsinOwnsIterableExpr`
- `StyioSecurityAstOwnership.InfiniteOwnsStartAndIncrementExprs`
- `StyioSecurityAstOwnership.AnonyFuncOwnsArgsAndThenExpr`

修复前析构计数均为 `0`。

## Decision

1. `CheckIsinAST` 增加 `iterable_owner_`，构造阶段接管 `Iterable` 生命周期。
2. `InfiniteAST` 增加 `start_owner_` / `inc_el_owner_`，保持 `getStart/getIncEl` raw 访问接口不变。
3. `AnonyFuncAST` 增加 `args_owner_` / `then_expr_owner_`，对外仍提供原有 getter。
4. 不改 public 构造签名，避免 parser/analyzer 调用点连锁修改。

## Alternatives

1. 继续依赖上层统一清理（边界不清晰且难定位泄漏源）。
2. 一次性改造 `ForwardAST/BackwardAST/CODPAST` 等整组高级节点（改动面过大，不符合微里程碑）。
3. 只修测试不改节点（不能降低真实泄漏风险）。

## Consequences

1. 三个高频小节点的生命周期闭环完成，泄漏面进一步缩小。
2. 所有权改造维持“接口兼容 + 内部收口”原则，便于持续小步合并。
3. 为后续 `ForwardAST/BackwardAST` 复杂节点改造提供可复用模式。
