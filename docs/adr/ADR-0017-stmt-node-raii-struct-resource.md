# ADR-0017: Stmt/Decl RAII 第四段（Struct/Resource 组合节点）

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

组合型节点仍存在裸指针集合：

1. `StructAST::{name, args}`
2. `ResourceAST::res_list`（`vector<pair<StyioAST*, string>>`）

这类节点承载多子节点集合，若不做内部所有权归集，析构顺序与泄漏风险在中断恢复场景下不可控。

## Decision

1. `StructAST` 内部新增 `unique_ptr<NameAST>` 与 `vector<unique_ptr<ParamAST>>`，对外保留 raw 视图字段。
2. `ResourceAST` 内部新增 owning 容器（`vector<pair<unique_ptr<StyioAST>, string>>`），对外继续提供 `res_list` raw 视图。
3. 在 `security` 增加 `StructOwnsNameAndParams` 与 `ResourceOwnsEntries` 回归测试。

## Alternatives

1. 维持裸指针集合并依赖全局 tracked cleanup。
2. 推迟到 C.5 再统一处理集合节点。

## Consequences

1. C.4 组合节点 ownership 边界收敛。
2. 对 parser/analyzer/codegen 的读取接口保持兼容。
3. 后续可把重心转向 D 阶段 soak/稳定性体系建设。
