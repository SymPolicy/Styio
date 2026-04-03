# ADR-0011: Expr 节点 RAII 第五段（ListOp/Attr）

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

`ListOpAST` 与 `AttrAST` 仍依赖裸指针保存子表达式：

1. `ListOpAST::{TheList, Slot1, Slot2}`
2. `AttrAST::{body, attr}`

这两类节点在集合访问与属性链解析中出现频繁，析构依赖外部协议，容易出现泄漏或边界不一致。

## Decision

1. `ListOpAST` 内部增加 `std::unique_ptr` owning `list/slot1/slot2`，外部通过现有 getter 读取 raw pointer。
2. `AttrAST` 内部增加 `std::unique_ptr` owning `body/attr`，保留现有 public raw 字段以兼容现有 codegen/tostring 访问路径。
3. 在 `security` 增加析构回归，覆盖 `ListOp` 的 1/2/3 子节点构造路径与 `AttrAST` 双子节点路径。

## Alternatives

1. 继续沿用裸指针，等待 C.4/C.5 统一回收。
2. 一次性迁移全部剩余 Expr 节点，扩大单次改动面。

## Consequences

1. C.3 Expr owning 迁移覆盖面继续扩大且保持小步可合并。
2. `ListOp/Attr` 生命周期与已迁移节点统一，降低重复释放/泄漏风险。
3. 对 parser/codegen 调用面保持兼容，无需额外 API 切换。
