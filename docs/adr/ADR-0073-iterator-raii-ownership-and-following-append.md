# ADR-0073: `IteratorAST` 所有权收口与 `following` 追加路径对齐

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0073: `IteratorAST` 所有权收口与 `following` 追加路径对齐.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

在 loop/streamzip 改造后，`IteratorAST` 仍是裸指针容器：

1. `collection`、`params`、`following` 仅保存 raw 指针；
2. parser 在 `parse_iterator_with_forward` 中直接对 `output->following.insert(...)` 追加节点；
3. 即使后续给 `IteratorAST` 引入 owner，若仍走直接 `insert`，也会出现 owner/view 脱钩。

红灯回归：

- `StyioSecurityAstOwnership.IteratorOwnsCollectionParamsAndFollowing`

修复前 collection/param/following 析构计数均为 `0`。

## Decision

1. `IteratorAST` 引入内部 owner：
   - `collection_owner_`
   - `params_owners_`
   - `following_owners_`
2. 构造阶段通过 `adopt_params` / `append_followings` 接管生命周期，并保留 `collection` / `params` / `following` raw view。
3. parser 侧 `parse_iterator_with_forward` 改为调用 `output->append_followings(...)`，禁止外部直接 `insert` 破坏 owner 同步。

## Alternatives

1. 仅改构造器，不改 parser 追加路径（会留下 owner/view 不一致窗口）。
2. 让 `following` 完全私有并全面替换调用点（改动面过大，不符合微里程碑节奏）。
3. 延后迭代器改造（M6/M7 高频路径继续泄漏）。

## Consequences

1. 迭代器节点生命周期闭环，构造与追加路径一致走 owning 逻辑。
2. `parse_iterator_with_forward` 的行为保持不变，但内存语义更稳定。
3. 为下一步 `BlockAST/MainBlockAST` 容器接管提供接口样板（“追加 API 而非外部直接改 raw 容器”）。
