# ADR-0072: `InfiniteLoopAST` / `StreamZipAST` 的 RAII 所有权收口

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0072: `InfiniteLoopAST` / `StreamZipAST` 的 RAII 所有权收口.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

在函数节点完成所有权改造后，循环与双流节点仍存在裸指针聚合：

1. `InfiniteLoopAST` 的 `while_cond_` 与 `body_` 仅保存原始指针；
2. `StreamZipAST` 的 `collection_a_/collection_b_`、`params_a_/params_b_`、`following_` 均为非 owning 容器；
3. 删除根节点时子树不会递归释放，且 `StreamZipAST` 包含参数与 body，泄漏面较大。

红灯回归可稳定复现：

- `StyioSecurityAstOwnership.InfiniteLoopOwnsWhileCondAndBodyNode`
- `StyioSecurityAstOwnership.StreamZipOwnsCollectionsParamsAndBody`

修复前出现：

- 条件表达式析构计数为 `0`；
- `tracked_node_count` 残留；
- stream zip 的 collection/param/body 析构计数均为 `0`。

## Decision

1. `InfiniteLoopAST` 引入内部 owner：
   - `std::unique_ptr<StyioAST> while_cond_owner_`
   - `std::unique_ptr<BlockAST> body_owner_`
2. `StreamZipAST` 引入内部 owner：
   - `collection_a_owner_` / `collection_b_owner_`
   - `params_a_owners_` / `params_b_owners_`
   - `following_owners_`
3. 对外兼容策略：
   - 保留原有 raw view 字段与 getter（`collection_*`、`params_*`、`following_`）；
   - 构造阶段通过 `adopt_*` 方法完成生命周期接管，不改调用签名。

## Alternatives

1. 继续依赖全局 tracked-node 清理（释放时机不明确，且不适合局部推理）。
2. 在 parser/analyzer 侧显式手动 delete（维护成本高，且与 checkpoint 原子化目标冲突）。
3. 先跳过循环/流节点，只改表达式节点（会在 M6/M7 高频路径保留泄漏）。

## Consequences

1. `InfiniteLoopAST` 与 `StreamZipAST` 子树释放语义稳定化，泄漏面继续收缩。
2. 新增两条安全回归作为中断恢复定位锚点（红灯书签）。
3. 保持 API 兼容，适配 trunk-based 小步合并策略。
