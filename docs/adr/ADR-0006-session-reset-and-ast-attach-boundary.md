# ADR-0006: CompilationSession 清理边界与 AST 挂载约束

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

在 C 阶段生命周期收敛中，`CompilationSession` 开始统一接管 Token/Context/AST/IR。  
如果在 `attach_ast()` 阶段做“全量 AST 清理”，会把刚解析出的新 AST 提前释放，导致后续类型推导/执行链路空输出或崩溃。  
此外，`Parser.hpp` 作为 `CompilationSession` 依赖头时，必须支持被测试目标独立包含。

## Decision

1. `CompilationSession::reset()` 作为唯一强清理边界：
   - 先释放 `IR`、`Context`、`ast_`；
   - 再执行 AST tracked 清理兜底；
   - 最后释放 token 容器。
2. `attach_ast()` 改为非破坏性挂载，不再基于全局计数做清理。
3. `Parser.hpp` 补齐直接依赖（`<regex>`、`AST.hpp`），保证可独立包含。
4. 安全回归新增会话测试，断言 `reset()` 后会话状态（tokens/context/ast/ir）全部清空。

## Alternatives

1. 在 `attach_ast()` 中继续按“全局 AST 计数”清理。
2. 完全移除 `CompilationSession` 的 AST 清理，仅依赖调用方手工释放。
3. 保持 `Parser.hpp` 依赖转译单元顺序（不做头自洽）。

## Consequences

1. 避免“挂载即释放”的生命周期错误，恢复里程碑与 pipeline 稳定性。
2. 会话清理契约更明确，便于中断恢复和后续 RAII 迁移。
3. 测试目标可直接覆盖会话逻辑，降低头文件包含顺序带来的隐式风险。
