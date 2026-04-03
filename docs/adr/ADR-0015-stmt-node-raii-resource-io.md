# ADR-0015: Stmt/Decl RAII 第二段（资源 IO 语句节点）

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

资源 IO 相关语句节点仍持有裸指针子节点：

1. `ReadFileAST::{varId, valExpr}`
2. `FileResourceAST::path_expr_`
3. `HandleAcquireAST::{var_, resource_}`
4. `ResourceWriteAST::{data_, resource_}`
5. `ResourceRedirectAST::{data_, resource_}`

这些节点是 M5 路径核心节点，所有权若不内聚会放大中断恢复后的生命周期不确定性。

## Decision

1. 上述节点全部改为内部 `unique_ptr` 持有子节点。
2. 保持现有 getter 返回 raw pointer，兼容 parser/typeinfer/codegen。
3. 在 `security` 新增 5 条析构回归，覆盖资源 IO 语句所有权契约。

## Alternatives

1. 保持裸指针并依赖外部删除协议。
2. 与 `Var/Param` 统一在下一刀一并迁移。

## Consequences

1. C.4 在资源语句路径的生命周期边界收敛。
2. M5 相关节点析构行为可通过回归测试稳定验证。
3. 后续可继续推进 `Var/Param` 节点簇所有权迁移。
