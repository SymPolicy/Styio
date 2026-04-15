# ADR-0084: State Inline InfiniteAST Clone 与 Soak Gate

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0084: State Inline InfiniteAST Clone 与 Soak Gate.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在单参数 state helper 内联路径中，`clone_state_expr_with_subst(...)` 先前未覆盖 `InfiniteAST`。  
当更新表达式为 `[...]` 时会触发：

- `Styio.TypeError: unsupported AST node in inlined state expression clone: 75`

同一表达式在非内联路径可执行，说明问题是内联克隆覆盖缺口而非语义本身不支持。

## Decision

1. 在 `clone_state_expr_with_subst(...)` 增加 `InfiniteAST` 克隆分支：
   - `InfiniteType::Original` 复制为 `new InfiniteAST()`。
   - `InfiniteType::Incremental` 递归克隆 `start/inc` 后构造新节点。
2. 新增 TDD 回归：
   - `StyioDiagnostics.StateInlineInfiniteLiteralFunctionUsesCallArgument`
   - 固化红灯（exit `4` + unsupported clone）到绿灯（exit `0`，输出 `0/0`）。
3. 同步增加 D.3 长跑守门：
   - `StyioSoakSingleThread.StateInlineInfiniteProgramLoop`
   - deep gate：`soak_deep_state_inline_infinite_program`（`STYIO_SOAK_STATE_INFINITE_ITERS=1500`）。

## Alternatives

1. 保持现状并在文档声明 `[...]` 不支持内联：
   - 拒绝。会造成内联/非内联行为分裂，违背 C.5 收口目标。
2. 仅补单测不加 soak：
   - 拒绝。该路径属于长期重构高频回归点，需 D.3 长跑兜底。

## Consequences

1. `state helper` 内联克隆覆盖面进一步扩展，`unsupported AST node` 触发概率下降。
2. `soak_smoke` 覆盖从 8 条提升到 9 条；`soak_deep` 新增独立 gate。
3. 中断恢复可直接用单测 + deep soak 双命令复核此链路。
