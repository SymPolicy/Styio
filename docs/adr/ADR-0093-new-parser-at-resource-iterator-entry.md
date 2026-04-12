# ADR-0093: New Parser 接管 `@file... >> ...` 资源流入口

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0093: New Parser 接管 `@file... >> ...` 资源流入口.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0092` 后，M7 的剩余入口级 fallback 继续收缩，但仍有一批 `@...` 起始资源流语句无法由 `new` 主入口接住：

1. `@file{...} >> #(x) => { ... }`
2. `@file{...} >> #(a) & @file{...} >> #(b) => { ... }`

这些语句已有稳定 legacy 分支，只是 `new` stmt subset 仍把 `TOK_AT` 限制在 `@[...]`。

## Decision

1. 新增可复用 helper `parse_at_stmt_or_expr(context)`，封装 legacy 的 `TOK_AT` 入口分发：
   - `@[...]`
   - `@(…)`
   - `@file{...}` / `@{...}` 加 postfix / iterator 尾缀
2. `parse_stmt_or_expr(...)` 与 `new` stmt subset 统一复用该 helper。
3. 这次只解决 `@...` 起始入口，不触碰 `[ ... ]` list-start iterator。
4. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnAtResourceSubsetSamples`
   - `StyioParserEngine.LegacyAndNewMatchOnM7ZipFilesSample`
   - `StyioParserEngine.LegacyAndNewMatchOnM7ArbitrageSample`
   - `StyioParserEngine.LegacyAndNewMatchOnM7FullPipelineSample`
   - `StyioParserEngine.ShadowArtifactDetailShowsZeroFallbackForAtResourceSubset`

## Alternatives

1. 继续让 `@file... >> ...` 通过入口级 fallback：
   - 拒绝。M7 的 file-stream 样例会长期维持非必要回退。
2. 与 `[ ... ] >> ...` list-start iterator 一起实现：
   - 拒绝。`[` 起始路径仍涉及 list literal / tuple / index / zip grammar，风险更高。

## Consequences

1. M7 的 `t05_zip_files`、`t08_arbitrage`、`t10_full_pipeline` 现在已经是零 fallback。
2. 剩余入口级 fallback 基本只集中在 `[ ... ] >> ...` list-start iterator。
3. E.7 的下一刀边界因此变得更清楚：只处理 list-start collection grammar，不再被 `@...` 入口分散注意力。
