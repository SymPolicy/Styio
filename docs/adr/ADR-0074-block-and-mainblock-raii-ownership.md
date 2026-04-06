# ADR-0074: `BlockAST` / `MainBlockAST` 的 RAII 所有权收口

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

`BlockAST` 与 `MainBlockAST` 是前端与中间表示的根容器节点，但此前仅保存裸指针：

1. `BlockAST::stmts` / `followings` 非 owning；
2. `MainBlockAST::Resources` / `Stmts` 非 owning；
3. parser 直接写 `block->followings = ...`，无法与未来 owner 同步。

红灯回归：

- `StyioSecurityAstOwnership.BlockOwnsStmtList`
- `StyioSecurityAstOwnership.MainBlockOwnsResourceAndStmtList`

修复前析构计数均为 `0`。

## Decision

1. `BlockAST` 新增 owner 容器：
   - `stmt_owners_`
   - `following_owners_`
2. `MainBlockAST` 新增 owner 容器：
   - `resources_owner_`
   - `stmt_owners_`
3. 保留 raw 视图兼容字段（`stmts` / `followings` / `Resources` / `Stmts`），构造阶段完成 ownership 接管。
4. 引入 `BlockAST::set_followings(...)`，并将 parser 写入路径切换到该 API，避免 owner/view 脱钩。

## Alternatives

1. 保持裸指针并依赖全局清理（根容器泄漏会放大整个子树泄漏）。
2. 在 parser 末端手动递归 delete（维护成本高且容易重复释放）。
3. 先不改容器节点（会阻塞 C.5 容器边界收口）。

## Consequences

1. 语句根容器释放语义稳定，AST 根节点 delete 不再遗漏整棵子树。
2. parser followings 追加路径变为显式 API，后续扩展更可控。
3. C.5 容器边界改造进入可持续迭代状态，可继续小步迁移剩余节点族。
