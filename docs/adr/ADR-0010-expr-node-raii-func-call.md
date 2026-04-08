# ADR-0010: Expr 节点 RAII 第四段（FuncCall）

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0010: Expr 节点 RAII 第四段（FuncCall）.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

`FuncCallAST` 长期使用裸指针保存 `callee/name/args`：

1. `func_callee`
2. `func_name`
3. `func_args`

在 parse-chain 重写 callee（链式调用）场景下，所有权边界不清晰，析构路径依赖外部约定，容易引发泄漏或重复释放。

## Decision

1. `FuncCallAST` 内部新增 `std::unique_ptr` 持有 `callee/name/args`。
2. 对外仍保留 raw-pointer 字段和访问方式，保证 parser/typeinfer/codegen 兼容。
3. 提供 `setFuncCallee(...)` 作为显式所有权移交入口，替代外部直接写成员指针。
4. 在 `security` 增加析构回归，覆盖：仅 name+args、callee+name+args、setter 接管 callee 三条路径。

## Alternatives

1. 继续保留裸指针并依赖外部统一释放。
2. 一次性将所有调用链相关节点做全量改造。

## Consequences

1. `FuncCallAST` 生命周期与已迁移 Expr 节点保持一致。
2. `parse_chain_of_call` 由“裸写指针”升级为“显式移交”。
3. C.3 可以继续按节点族小步推进，测试与回滚面保持可控。
