# ADR-0118: Nightly-First Completion Boundary for E.7 and F.3

## Status

Accepted

## Context

`E.7` 的原始表述偏向“彻底删除 legacy 主路径”，但当前代码库已经形成了更稳妥的工程边界：

1. 默认 CLI、five-layer pipeline、shadow gate、恢复脚本和主验证链路都已经以 `nightly` 为第一入口；
2. `legacy` 仍然保留在 parser core 与 parity/compare harness 中，用于差异对照、回归定位和显式回退；
3. `m1/m2/m5/m7` 已有 zero-fallback / zero-internal-bridges gate；
4. `parser_legacy_entry_audit` 已冻结“主入口不可回连 legacy”的工程约束。

继续为了“名义上的删除”而一次性移除所有 `legacy` 代码，收益很低，风险很高，也不符合 checkpoint 微型化原则。

同时，`F.3` 的最后尾项也不是功能本身，而是维护入口标准和恢复脚本的可靠性，例如失效 `build/` cache 不应让 `checkpoint-health` 直接失效。

## Decision

1. `E.7` 完成的工程判定改为：
   - 默认主路径 `nightly-first`；
   - `legacy` 仅保留在 parser core 与显式 parity harness；
   - shadow gate / entry audit / five-layer gate 全绿；
   - 不再要求在同一里程碑里物理删除所有 `legacy` 代码。
2. `F.3` 完成的工程判定改为：
   - 入口标准已冻结到 workflow 文档；
   - `checkpoint-health.sh` 能自动选择可工作的 build 目录；
   - 主恢复命令不依赖人工清理陈旧 cache；
   - parser/nightly-first 的 gate 能一键复现。
3. 真正的 `legacy` 代码删除，若未来要做，单列为新的维护性 checkpoint，而不是继续挂在 `E.7/F.3` 名下。

## Alternatives

1. 继续坚持“E.7 只有删光 legacy 才算完成”。
   - 问题：会把本应独立的小风险清理动作绑成一个高风险黑盒尾项。
2. 不写清完成边界，只在 history 里口头说明。
   - 问题：未来恢复时无法判断“还剩什么没做”，又会重新制造上下文丢失。

## Consequences

正面：

1. 剩余里程碑可以用现有 gate 和 workflow 脚本直接判定完成。
2. `legacy` 被限制在明确的隔离区，不再污染默认主路径。
3. 恢复脚本从“依赖环境记忆”变成“自动绕过坏 cache”的可靠入口。

负面：

1. 仓库里仍保留 `legacy` 代码，体积与维护成本不会立刻归零。
2. 若未来要完全删除 `legacy`，需要单独再开一个 checkpoint。
