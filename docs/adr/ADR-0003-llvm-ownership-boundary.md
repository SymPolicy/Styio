# ADR-0003: LLVM 对象保持非托管边界

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

Styio 正在推进 RAII 改造（CompilationSession），但 LLVM IR 对象生命周期本身由 LLVM 容器管理（`LLVMContext`、`Module`、`IRBuilder` 体系）。

## Decision

1. `llvm::Value*`、`llvm::Type*` 在 Styio 中视为 **non-owning** 句柄，不包裹为 owning 智能指针。
2. LLVM 生命周期由 `StyioToLLVM` 持有并统一销毁。
3. CompilationSession 仅接管 Token/Context/AST/StyioIR 生命周期，不接管 LLVM 节点所有权。

## Alternatives

1. 将 LLVM 结点包装为自定义 RAII 拥有者。
2. 在多个模块分散持有 `LLVMContext`/`Module`。

## Consequences

1. 降低 double-free / dangling pointer 风险。
2. 需要清晰文档化跨层边界，避免误用“看似可 delete 的 llvm 指针”。
