# ADR-0082: State Inline 克隆补齐 MatchCases/Cases

- Status: Accepted
- Date: 2026-04-07

## Context

在单参数 state helper 内联路径中，`resolve_state_decl_impl` 会通过 `clone_state_expr_with_subst(...)` 克隆并替换状态更新表达式。  
当更新表达式包含 `?= { ... }`（`MatchCasesAST/CasesAST`）时，克隆函数缺少对应分支，会抛出：

- `Styio.TypeError: unsupported AST node in inlined state expression clone`

这会导致本应可执行的内联 state 程序在运行时失败。

## Decision

1. 在 `clone_state_expr_with_subst(...)` 增加：
   - `CasesAST` 克隆分支（逐条克隆 case pair 与 default）。
   - `MatchCasesAST` 克隆分支（克隆 scrutinee + cloned cases）。
2. 维持原有错误边界：若 `MatchCasesAST` 克隆后未得到 `CasesAST`，抛 `TypeError`。
3. 新增回归测试：
   - `StyioDiagnostics.StateInlineMatchCasesFunctionUsesCallArgument`
   - 验证 state helper 在 `?=` 更新表达式中可稳定输出 `10/12/15`，且不再出现 unsupported clone 文案。

## Alternatives

1. 不支持 `MatchCases` 内联，仅文档声明限制：
   - 拒绝。该语法已在主语言表达式中可用，内联路径拒绝会造成行为割裂。
2. 在内联前直接禁止 `MatchCases`：
   - 拒绝。属于能力回退，且会增加用户侧心智负担。

## Consequences

1. state helper 内联覆盖面从基础算术扩展到 `match` 表达式更新场景。
2. `unsupported AST node` 触发概率进一步降低，C.5 的 clone 覆盖目标继续收敛。
3. 新回归已进入 `styio_pipeline`，可作为中断恢复坐标直接复现。
