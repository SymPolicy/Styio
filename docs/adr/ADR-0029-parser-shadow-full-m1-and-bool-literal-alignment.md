# ADR-0029: Parser Shadow 全量 M1 门槛与 Bool Literal 对齐

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0029: Parser Shadow 全量 M1 门槛与 Bool Literal 对齐.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

`--parser-shadow-compare` 已在核心样例集可用，但 E.5 下一段需要把门槛从“核心样例”提升到 “M1 全量样例”，避免子集扩展后出现未覆盖的 AST 结构漂移。

在扩展到全量 M1 时，`t18_bool.styio` 暴露出 legacy/new 的历史输出差异：legacy 把 `true/false` 归并为 `BoolAST`，new 子集此前按普通 `NameAST` 处理。

## Decision

1. 在 `NewParserExpr` 的 `NAME` primary 路径增加 bool literal 分支：
   - `true`/`false` 直接构造 `BoolAST`。
2. 在流水线测试中新增 `StyioParserEngine.ShadowCompareAcceptsM1FullSuite`：
   - 遍历 `tests/milestones/m1/t*.styio`；
   - 对每个样例分别执行 `--parser-engine=legacy|new --parser-shadow-compare`；
   - 要求双路径都返回成功。
3. 保持默认行为不变：
   - 未开启 `--parser-shadow-compare` 时不增加额外开销；
   - parser 默认引擎仍保持 `legacy`。

## Alternatives

1. 仅维持核心样例集，不扩展到 M1 全量。
2. 将 bool literal 差异留给后续统一语义重整再修。
3. 直接切换默认引擎到 `new`，再通过用户反馈修补差异。

## Consequences

1. E.5 的 shadow compare 覆盖从“抽样”提升到 “M1 全量”。
2. new 子集在 bool literal 上与 legacy 行为对齐，减少伪差异。
3. 为 E.6 默认切换门槛提供更稳的基线（先保障全量 M1 无差异）。
