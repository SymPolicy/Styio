# ADR-0091: New Parser 接管 Name-Iterator 与 Instant Pull 子集

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0091: New Parser 接管 Name-Iterator 与 Instant Pull 子集.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0090` 后，M5 的资源写入/重定向已经不再回退，但以下两类语法仍会把 `new` 主路径打回 legacy：

1. `name >> #(x) => { ... }` 这类 name-led iterator 语句；
2. `(<< @file{...})` 这类 instant pull 表达式。

它们都属于范围较小的“叶子语法”，不值得为了去掉入口级 fallback 立刻重写完整 stream grammar。

## Decision

1. 在 `parse_stmt_new_subset(...)` 的 name 探测分支中，新增：
   - `name >> ...` 直接复用现有 `parse_iterator_only(context, NameAST::Create(name))`
2. 在 `parse_expr_new_subset(...)` 的 `(` primary 分支中，新增：
   - `(<< @file{...})` / `(<< @{...})` 直接构造 `InstantPullAST`
3. 这次只复用 legacy 的稳定叶子 helper：
   - iterator body 与 instant pull 的资源解析沿用现有实现；
   - 不把 `@file... >> ...`、`[1,2,3] >> ...` 等 collection-start iterator 一并拉进来。
4. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnIteratorStmtSubsetSamples`
   - `StyioSecurityNewParserStmt.MatchesLegacyOnInstantPullSubsetSamples`
   - `StyioParserEngine.LegacyAndNewMatchOnM5ReadFileSample`
   - `StyioParserEngine.LegacyAndNewMatchOnM5AutoDetectSample`
   - `StyioParserEngine.LegacyAndNewMatchOnM5PipeFuncSample`
   - `StyioParserEngine.LegacyAndNewMatchOnM7InstantPullSample`
   - `StyioParserEngine.ShadowArtifactDetailShowsZeroFallbackForIteratorSubset`

## Alternatives

1. 继续让这两类语法完全依赖入口级 fallback：
   - 拒绝。M5 样例会持续显示非必要回退。
2. 直接实现完整 collection-start iterator grammar：
   - 拒绝。范围过大，会把 `[ ... ]` / `@file{...}` / zip / forward 一起卷进本 checkpoint。

## Consequences

1. M5 的 `read_file` / `auto_detect` / `pipe_func` 样例现在全部可以在入口级 route stats 上做到 `legacy_fallback_statements=0`。
2. M7 的 instant pull 样例 `t04_instant_pull` 也达到了零回退。
3. 剩余主要 fallback 已收缩到 collection-start iterator 与 `@[...]` 语句。
