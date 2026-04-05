# ADR-0055: Match Scrutinee 整型边界与崩溃防线

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

在资源流迭代场景中，`line ?={ ... }` 会把字符串/指针类型作为 `match` 的 scrutinee 下放到 `SGMatch`。  
原实现在 `StyioToLLVM::toLLVMIR(SGMatch*)` 对 scrutinee 无条件走 `CreateSExtOrTrunc(..., i64)`，当输入是指针类型时触发 LLVM 断言并导致进程 `Abort trap`（退出码 `134`），破坏“错误可诊断、进程不崩溃”的目标。

## Decision

1. 在 `toLLVMIR(SGMatch*)` 内新增 `coerce_match_scrutinee_to_i64` 统一转换边界：
   - `i64`：直接使用；
   - 其它整数类型（如 `i1/i32`）：`SExtOrTrunc` 到 `i64`；
   - 非整数类型（指针/浮点等）：抛 `StyioTypeError("match scrutinee must be integer-typed")`。
2. 在 statement/expression 两条 `SGMatch` lowering 路径复用同一边界函数。
3. 将 codegen 阶段抛出的 `StyioTypeError` 在 CLI 主流程映射为 `TypeError`（退出码 `4`）。
4. 增加回归测试 `StyioParserEngine.PointerScrutineeMatchDoesNotAbortAndReportsTypeError`，冻结行为为：
   - 不再 abort；
   - 双引擎返回 `TypeError`（退出码 `4`）；
   - JSONL 诊断含稳定错误信息。

## Alternatives

1. 对指针执行 `ptrtoint` 后继续 `switch`，允许按地址匹配。
2. 对浮点/指针执行隐式数值转换，维持“尽量跑通”。
3. 保持现状，依赖 LLVM 断言暴露错误。

## Consequences

1. `match` 的可执行边界更明确：当前仅支持整型 scrutinee，不再因类型不匹配导致进程级崩溃。
2. 错误从“不可恢复崩溃”收敛为“可测试、可诊断、可回归”的稳定输出。
3. 若后续要支持字符串/枚举语义匹配，可在该边界函数上扩展而不改变现有失败契约。
