# ADR-0008: Expr 节点 RAII 第二段（CondAST）

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0008: Expr 节点 RAII 第二段（CondAST）.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

在 `BinOpAST`/`BinCompAST` 完成 RAII 后，逻辑表达式节点 `CondAST` 仍使用裸指针保存子表达式。  
这会让布尔表达式链路继续依赖外部释放约定，不利于 C.3 持续推进。

## Decision

1. `CondAST` 内部改为 `std::unique_ptr` owning：
   - 单目路径：`value_owner_`；
   - 双目路径：`lhs_owner_` / `rhs_owner_`。
2. 对外继续保留原有 raw-pointer 访问接口（`getValue/getLHS/getRHS`）。
3. 新增安全回归，验证 unary/binary 两种 Cond 析构时都会释放子表达式。

## Alternatives

1. 维持 `CondAST` 裸指针，延后到 C.4 统一处理。
2. 一次性改造所有 Expr 节点为 owning 并同步改写所有调用点。

## Consequences

1. C.3 继续按“单节点族可合并切片”推进，风险可控。
2. 逻辑表达式链路的生命周期约束清晰化。
3. 后续迁移可复用相同模式：内部 owning，外部兼容接口。
