# ADR-0078: StateDecl 所有权收口与 inlining 克隆替换边界

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0078: StateDecl 所有权收口与 inlining 克隆替换边界.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-06

## Context

`StateDeclAST` 之前仍以裸指针持有 `window/acc/export/update` 子节点。  
同时 `ToStyioIR::resolve_state_decl_impl` 在 inlining 路径里通过 `StateDeclAST::Create(...)` 直接复用原 `StateDeclAST` 的部分子指针，只替换 `update_expr`，这与 RAII 迁移天然冲突：

- 一旦 `StateDeclAST` 改为 owning，复用旧子树会触发双释放风险；
- 仅靠“约定不删”无法满足 C.4/C.5 的可验证内存安全目标。

## Decision

1. `StateDeclAST` 全量转为 owner/view：
   - 新增 `window_header_owner_` / `acc_name_owner_` / `acc_init_owner_` / `export_var_owner_` / `update_expr_owner_`；
   - 对外 getter 维持原签名，调用侧最小改动。

2. `ToStyioIR` 的 state inlining 改为“克隆后替换”：
   - 新增 `clone_state_expr_with_subst(...)`；
   - 参数替换不再返回原子树指针，改为克隆替换树；
   - `resolve_state_decl_impl` 构造内联 `StateDeclAST` 时，`window/acc/export` 也走克隆，避免与源节点共享所有权。

3. 新增 TDD 书签回归：
   - `StyioSecurityAstOwnership.StateDeclOwnsAccInitExportVarAndUpdateExpr`。

## Alternatives

1. 继续保留 `StateDeclAST` 裸指针，延后到后续批次：
   - 被拒绝。该节点是 M6 主路径核心，延后会持续放大 ownership 混用风险。

2. 只让 `StateDeclAST` 部分字段 owning（例如仅 `update_expr`）：
   - 被拒绝。语义复杂度上升且难以证明“哪些字段可共享、哪些不可共享”。

3. inlining 继续复用旧指针并依赖“不会 delete”的隐式约定：
   - 被拒绝。与 checkpoint 中“可验证可回滚”目标冲突。

## Consequences

1. `StateDeclAST` 生命周期与其它已迁移 AST 家族对齐。
2. state inlining 路径不再共享子树，降低后续 C.5 深化迁移时的 UAF/DF 风险。
3. `security` 标签用例数增至 110，并在常规与 ASan/UBSan 构建下全绿。
