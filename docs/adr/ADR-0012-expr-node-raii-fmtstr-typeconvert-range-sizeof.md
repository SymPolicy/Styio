# ADR-0012: Expr 节点 RAII 第六段（FmtStr/TypeConvert/Range/SizeOf）

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

下列表达式节点仍使用裸指针保存子表达式：

1. `FmtStrAST::Exprs`
2. `TypeConvertAST::Value`
3. `RangeAST::{StartVal, EndVal, StepVal}`
4. `SizeOfAST::Value`

这些节点都属于“构造后只读”的所有权模型，适合以小步方式改为 RAII，降低泄漏与析构边界不一致风险。

## Decision

1. 对以上 4 个节点引入内部 `std::unique_ptr` owning（`FmtStrAST` 为 `vector<unique_ptr<StyioAST>>`）。
2. 对外继续保留现有 getter 的 raw-pointer/向量视图，确保 parser/typeinfer/codegen 调用面保持兼容。
3. 在 `security` 新增 4 条析构回归：`FmtStr`、`TypeConvert`、`Range`、`SizeOf`。

## Alternatives

1. 维持裸指针，等待后续统一内存收口。
2. 一次性处理更多集合节点（`Tuple/List/Set`），扩大单次变更面。

## Consequences

1. C.3 持续保持“小节点族+可回滚”的节奏。
2. 4 个高频表达式节点的生命周期与现有 RAII 节点保持一致。
3. 后续可继续迁移 `Tuple/List/Set` 等集合节点，或转入 C.4 Stmt/Decl 阶段。
