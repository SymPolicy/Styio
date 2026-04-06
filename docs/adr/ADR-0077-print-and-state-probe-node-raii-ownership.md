# ADR-0077: Print/State Probe 节点所有权收口

- Status: Accepted
- Date: 2026-04-06

## Context

在前一批 C.3/C.5 收口后，以下节点仍使用裸指针持有子节点，生命周期边界不清晰：

- `PrintAST`
- `StateRefAST`
- `HistoryProbeAST`
- `SeriesIntrinsicAST`

这几类节点位于语句输出与 M6 状态表达式链路中，若继续保留裸指针，会让后续 `StateDeclAST` 收口前的析构语义持续不一致，且难以通过统一 ownership 测试门禁。

## Decision

1. 对上述四类节点统一采用 owner/view 双轨：
   - `PrintAST`：新增 `expr_owners_`，保留 `exprs` 作为只读视图；
   - `StateRefAST`：新增 `name_owner_`；
   - `HistoryProbeAST`：新增 `target_owner_` / `depth_owner_`；
   - `SeriesIntrinsicAST`：新增 `base_owner_` / `window_owner_`。

2. 保持外部访问接口不变（`get*` 与 `exprs` 视图不改名），避免牵连 parser/analyzer 调用面。

3. 以 TDD 书签补齐 4 条析构回归：
   - `StyioSecurityAstOwnership.PrintOwnsExpressionList`
   - `StyioSecurityAstOwnership.StateRefOwnsNameNode`
   - `StyioSecurityAstOwnership.HistoryProbeOwnsTargetAndDepth`
   - `StyioSecurityAstOwnership.SeriesIntrinsicOwnsBaseAndWindow`

4. 明确暂缓 `StateDeclAST` 迁移：`ToStyioIR::subst_param_in_expr` 仍存在“重建父节点 + 复用部分旧子节点”的路径，需先完成替换表达式克隆策略再推进，避免 owner 后的共享子树双释放风险。

## Alternatives

1. 等 `StateDeclAST` 一并改造时再处理这四个节点：
   - 被拒绝。会让当前树中 ownership 语义继续混用，难以做渐进式可合并 checkpoint。

2. 本批直接连 `StateDeclAST` 一起迁移：
   - 被拒绝。需要先处理 `subst_param_in_expr` 的共享子树问题，直接迁移风险过高，不符合 1-3 天 checkpoint 原则。

## Consequences

1. `Print` 与状态探针表达式节点析构语义与其余已迁移 AST 家族对齐。
2. `StyioSecurityAstOwnership` 覆盖面扩展到输出与状态探针节点，`security` 标签回归从 105 条增至 109 条并保持全绿。
3. C.4/C.5 后续焦点可收敛到 `StateDeclAST` + 替换表达式克隆策略这一高风险边界，降低大步迁移失败概率。
