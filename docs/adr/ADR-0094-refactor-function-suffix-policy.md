# ADR-0094: Refactor Function Suffix Policy (`_legacy` / `_nightly` / `_latest` / `_draft`)

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0094: Refactor Function Suffix Policy (`_legacy` / `_nightly` / `_latest` / `_draft`).

**Last updated:** 2026-04-08

**Status:** Accepted

## Context

Styio 当前处于长期的 trunk-based 微里程碑重构期。parser、runtime、state-inline 和后续模块会长期处于双轨并存状态。

此前同一语义域内混用 `legacy` / `new` / 无后缀函数名，带来三个问题：

1. 中断恢复时，无法从函数名直接判断该实现是稳定路径、影子路径还是共享 helper；
2. `new` 与默认引擎切换后语义已经不准确，后续会稳定演变为 nightly 线路；
3. 未来继续拆 checkpoint 时，尚未完成的在改函数与已可合并函数缺少显式边界。

## Decision

自 2026-04-07 起，进入双轨重构流程的函数统一采用以下命名规则：

1. `*_legacy`
   - 稳定旧实现。
   - 默认优先保证兼容、可回滚、可替换。
2. `*_nightly`
   - 原 `new` 路径的新实现。
   - 用于影子对比、增量接管和夜间线路演进。
3. `*_latest`
   - `legacy` 与 `nightly` 共同依赖的稳定共享入口或公共 helper。
   - 仅当接口已对双轨都成立时才允许升格为 `_latest`。
4. `*_draft`
   - 已进入改造、但尚未满足 checkpoint 合并门槛的在改版本。
   - 典型形式为 `parse_stmt_or_expr_legacy_draft` 或 `parse_zip_head_nightly_draft`。

附加约束：

1. 文档、测试、CLI 开关统一使用 `nightly`，不再使用 `new` 作为活动命名。
2. 如需兼容历史接口，可保留 `new -> nightly` 的输入别名，但必须明确标记为 deprecated compatibility alias。
3. 历史未进入重构的旧函数可暂时保留原名；一旦开始改造，首个提交必须切入上述后缀体系。

## Alternatives

1. 保持 `legacy/new`：
   - 问题：`new` 是时间性命名，无法表达长期维护状态，也与默认引擎切换后的语义不一致。
2. 只在文档里约定，不改函数名：
   - 问题：中断恢复时必须靠上下文记忆，无法从代码直接读出状态。
3. 对全仓函数一次性机械改名：
   - 问题：改动面过大，收益与风险不匹配，也会阻断当前里程碑推进。

## Consequences

1. parser 等双轨模块的共享入口会逐步迁移到 `_latest`。
2. 新实现一律以 `_nightly` 命名，`new` 仅作为兼容别名存在。
3. 后续 checkpoint 文档必须记录哪些函数处于 `_draft`，以及何时升格为 `_latest`。
4. 历史 ADR / history 中出现的 `new parser` 术语保留原文，但在当前流程中一律解释为 `nightly parser`。
