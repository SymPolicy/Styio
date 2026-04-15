# ADR-0115: Nightly Wave Safe Fallback 与 Bool Literal Name-Start 修正

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0115: Nightly Wave Safe Fallback 与 Bool Literal Name-Start 修正.

**Last updated:** 2026-04-08

## Context

在 `ADR-0085` 将默认 parser 引擎切到 `nightly` 之后，里程碑套件暴露出两个独立问题：

1. `M4` 的 wave 语法：

```styio
result = (a > b) <~ a | b
(x > 50) ~> >_("big") | >_("small")
```

会被 `nightly` 的表达式子集截断为前半段表达式，然后把 `<~` / `~>` 误留给下一条语句，最终报出 `No Statement Starts With This`。

2. `M9` 的标准流布尔写出：

```styio
true >> @stdout
```

会被 `nightly` 的 NAME-led 语句快捷路径当成变量名 `true`，而不是布尔字面量，最终输出 `0` 而不是 `true`。

这两个问题的风险性质不同：

1. wave 语法当前还没有进入 `nightly` 子集的受控覆盖面；
2. `true/false` 作为表达式字面量，本来就应该由 `nightly` 原生吃掉。

## Decision

1. 在 `NewParserExpr.cpp` 中，将 `<~` / `~>` 明确加入 `nightly` 表达式子集的 `unsupported continuation` 集合。
   - 这样 `nightly` 在遇到 wave 续写时会主动抛出稳定异常；
   - 外层逐语句路由随后安全回退到 `legacy`，而不是留下半条语句造成误解析。
2. 同时把 `=`, `:=`, `:`, `<-`, `<<`, `+=` 等 statement-only 续写也纳入同一拒绝集合，避免 `nightly` 对 bool/name 起始语句发生“部分吞掉后静默返回”。
3. 在 NAME-led 语句快捷路径中，`true` / `false` 不再按变量名走 bind/resource/iterator 探测，而是直接交回 `parse_expr_subset_nightly(...)`。
   - 这让 `true >> @stdout`、`false -> @stderr` 之类表达式起始语句重新回到字面量语义。
4. 用 parser-engine 回归冻结这两个边界：
   - `LegacyAndNightlyMatchOnM4WaveMergeSample`
   - `LegacyAndNightlyMatchOnM4WaveDispatchSample`
   - `LegacyAndNightlyMatchOnM9StdoutBoolSample`

## Consequences

正面：

1. 默认 `nightly` 引擎下，`M4` wave 语法恢复可用，但没有假装已经进入零 fallback 覆盖面。
2. `M9` 的 `true >> @stdout` 恢复正确输出 `true`。
3. `nightly` 的 NAME-led 语句路径不再把布尔字面量误降级成变量名。
4. 里程碑套件重新回到 `114/114` 全绿，`checkpoint-health` 继续全绿。

负面：

1. `M4` wave 语法目前仍依赖 `legacy` 执行，这意味着 `E.7` 还没有把这部分 grammar 真正迁入 `nightly`。
2. `unsupported continuation` 列表继续增长，需要在未来真正接管 wave grammar 时及时删掉对应拒绝项，避免形成永久性 fallback。
