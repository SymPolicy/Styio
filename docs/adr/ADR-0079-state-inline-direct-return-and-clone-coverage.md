# ADR-0079: 单参数 state helper 内联识别修复与克隆覆盖扩展

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0079: 单参数 state helper 内联识别修复与克隆覆盖扩展.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-06

## Context

`ToStyioIR` 的 pulse state 内联链路之前只接受一种 helper 形态：

- `SimpleFuncAST` 的返回值必须是 `BlockAST`；
- 且 block 内必须只有一条 `StateDeclAST`。

这与当前 parser 的主流写法不匹配。`# pulse = (x) => @[sum = 0](out = $sum + x)` 会生成“直接返回 `StateDeclAST`”的 simple func。结果是：

1. `stmt_may_contain_pulse_state` 无法识别该语句为 pulse state；
2. 状态计划不会建立，调用参数不进入 state 更新表达式；
3. 行为退化为错误结果（例如本应输出 `1/3/6`，实际可能固定为 `0/0/0`）。

同时，`clone_state_expr_with_subst` 在 C.5 路径里仍有较窄覆盖面，部分状态表达式/语句节点会落入 `unsupported AST node in inlined state expression clone`。

## Decision

1. 引入统一识别函数 `simple_func_returns_single_state_decl(...)`：
   - 接受两种合法返回形态：
     - 直接 `StateDeclAST`；
     - `BlockAST` 且仅一条 `StateDeclAST`。
   - 在 `stmt_may_contain_pulse_state` 与 `resolve_state_decl_impl` 共用同一规则，避免检测/执行边界不一致。

2. 扩展 state inlining 克隆覆盖：
   - 新增支持 `WaveDispatchAST`、`FmtStrAST`、`ReturnAST`、`PrintAST`、`PassAST`、`BreakAST`、`ContinueAST`、`BlockAST`。

3. 新增 TDD 回归书签：
   - `StyioDiagnostics.SingleArgStateFunctionInliningUsesCallArgument`：
     - 输入 `# pulse = (x) => @[sum = 0](out = $sum + x)` + pulse 调用；
     - 固化输出 `1\n3\n6\n`，确保调用参数真正参与 state 更新。

## Alternatives

1. 继续只支持 block-return state helper：
   - 被拒绝。与现有 simple func 语法主路径冲突，导致静默行为错误。

2. 仅在 `resolve_state_decl_impl` 放宽规则，不改 `stmt_may_contain_pulse_state`：
   - 被拒绝。会出现“检测不到但执行期可内联”的不一致，增加维护复杂度。

3. 不扩展克隆覆盖，仅修复 helper 识别：
   - 被拒绝。会保留 C.5 的已知高频失败入口，恢复后仍可能触发 unsupported 分支。

## Consequences

1. 单参数 state helper 的直返 `StateDecl` 写法恢复为可识别、可内联、可验证。
2. `ToStyioIR` 的 state inlining 链路在更多合法 AST 形态下可持续运行，降低 C.5 中断恢复成本。
3. 新增回归在常规与 ASan/UBSan 构建下均通过，且 `styio_pipeline/security/milestone` 组测保持全绿。
