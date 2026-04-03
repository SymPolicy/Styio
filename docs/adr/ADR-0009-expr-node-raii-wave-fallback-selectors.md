# ADR-0009: Expr 节点 RAII 第三段（Wave/Fallback/Selectors）

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

`CondAST` 迁移后，M4 相关表达式节点仍有裸指针子树：

1. `WaveMergeAST`
2. `WaveDispatchAST`
3. `FallbackAST`
4. `GuardSelectorAST`
5. `EqProbeAST`

这些节点在解析链路里高频出现，继续依赖外部释放约定会放大生命周期风险。

## Decision

1. 将以上 5 个节点改为内部 `std::unique_ptr` owning 子表达式。
2. 对外 getter 保持 raw-pointer 兼容，避免影响 parser/typeinfer/codegen 调用面。
3. 在安全测试中新增析构回归，覆盖每个节点的 child ownership 契约（含 `WaveDispatch`）。

## Alternatives

1. 保持裸指针并等待 C.4 统一处理。
2. 一次性改造全部剩余 Expr 节点（更大变更面）。

## Consequences

1. C.3 以小步方式继续推进，合并风险可控。
2. M4 表达式链路的生命周期一致性进一步提高。
3. 后续可按同样模式继续迁移 `WaveDispatchAST` 等剩余节点。
