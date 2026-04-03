# ADR-0016: Stmt/Decl RAII 第三段（Var/Param 家族）

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

`VarAST` 及其派生节点长期通过裸指针承载声明信息：

1. `VarAST::{var_name, var_type, val_init}`
2. `ParamAST` 派生字段映射同一组指针
3. `OptArgAST/OptKwArgAST` 依赖 `VarAST` 基类构造

这些节点被 Parser/TypeInfer/ToStyioIR 高频访问，若所有权边界不清晰会导致泄漏或析构不确定。

## Decision

1. 在 `VarAST` 内部引入 `unique_ptr` 持有 `name/type/init`，并保持现有 public raw 字段兼容读取。
2. 维持 `ParamAST/OptArgAST/OptKwArgAST` 现有接口和字段，不改调用面，仅借由基类统一接管生命周期。
3. 在 `security` 新增 `Var/Param/OptArg/OptKwArg` 析构回归，锁定继承链所有权契约。

## Alternatives

1. 继续保持裸指针并依赖外部释放。
2. 先重构 `ParamAST` 字段布局，再统一迁移（单次风险更高）。

## Consequences

1. Var 家族节点生命周期收口到基类，减少派生层重复管理风险。
2. 现有 parser/typeinfer/codegen 调用面保持兼容。
3. C.4 可继续推进到 `Struct/Resource` 组合型节点。
