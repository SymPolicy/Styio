# ADR-0076: Forward/Backward/CODP 与 Stream 节点所有权收口

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0076: Forward/Backward/CODP 与 Stream 节点所有权收口.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-06

## Context

C.3/C.5 之前批次已经完成了主干表达式与函数/块节点的 RAII 接管，但以下节点仍保留裸指针成员，存在生命周期不清和泄漏风险：

- `SnapshotDeclAST` / `InstantPullAST`
- `IterSeqAST`（`hash_tags`）
- `ExtractorAST`
- `BackwardAST`
- `CODPAST`
- `ForwardAST`（默认 block 与 `setRetExpr` 路径）

同时在 C.6 sanitizer 验证中，发现 `parse_iterator_only` 将 `parse_iterator_tail` 的结果强转为 `IteratorAST*`，在 `StreamZipAST` 路径触发 UBSan downcast 异常。

## Decision

1. AST 所有权收口：
   - `SnapshotDeclAST`：新增 `var_owner_`/`resource_owner_`。
   - `InstantPullAST`：新增 `resource_owner_`。
   - `IterSeqAST`：新增 `hash_tag_owners_`，保留 `hash_tags` 只读视图。
   - `ExtractorAST`：新增 `tuple_owner_`/`op_owner_`；`Create` 统一为 `static`。
   - `BackwardAST`：新增 `object_owner_`/`params_owner_`/`operation_owners_`/`ret_expr_owners_`。
   - `CODPAST`：新增 `op_arg_owners_` 与 `next_op_owner_`，定义 `PrevOp` 为 non-owning，提供 `setNextOp(...)` 管理链式所有权。
   - `ForwardAST`：新增默认 `block_owner_` 和 `ret_expr_owner_`，`setRetExpr(...)` 转为显式接管。

2. Parser 边界修正：
   - `parse_iterator_only` / `parse_iterator_with_forward` 返回类型改为 `StyioAST*`；
   - 仅在真实 `IteratorAST` 上追加 forward；
   - `parse_codp` 改为 `prev_op->setNextOp(curr_op)`，避免裸指针链路失配。

3. 测试准入（TDD 书签）：
   - 新增 7 条 `StyioSecurityAstOwnership.*` 回归：
     - `SnapshotDeclOwnsVarAndResource`
     - `InstantPullOwnsResource`
     - `IterSeqOwnsHashTags`
     - `ExtractorOwnsTupleAndOperation`
     - `BackwardOwnsObjectParamsOperationsAndReturns`
     - `CodpOwnsArgsAndNextChain`
     - `ForwardSetRetExprTakesOwnership`

## Alternatives

1. 继续维持裸指针 + 约定式手动释放：
   - 被拒绝。所有权跨层传播后容易产生遗漏与二次释放风险。

2. 在 `CODPAST` 上双向 owning（`PrevOp` 与 `NextOp` 都 owning）：
   - 被拒绝。链结构会形成释放环/重复释放风险。

## Consequences

1. `CODPAST` 删除根节点即可级联释放参数与 next 链，`PrevOp` 保持只读回溯语义。
2. `StreamZip`/`IterSeq`/`Iterator` 在 parser 分派层不再依赖未定义 downcast 行为。
3. sanitizer 回归恢复稳定：
   - `build-asan-ubsan` 上 `security`（105/105）与 `styio_pipeline`（29/29）通过。
