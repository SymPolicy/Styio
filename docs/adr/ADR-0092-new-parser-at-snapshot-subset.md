# ADR-0092: New Parser 接管 `@[...]` Snapshot / State Decl 起始子集

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0091` 之后，M7 里剩余的大头 fallback 已集中到两类起始符：

1. `@[...]` snapshot / state decl 起始语句；
2. `@file{...}` / `[1,2,3]` 这类 collection-start iterator。

前者边界清晰，且已有 legacy helper `parse_state_decl_after_at(...)`；后者仍涉及完整 iterator / zip grammar，不适合和 `@[...]` 混在同一切片。

## Decision

1. 将 `parse_state_decl_after_at(...)` 从 `Parser.cpp` 内部静态 helper 提升为可复用接口。
2. `new` stmt subset 新增 `TOK_AT` 起始支持，但只接管 `@[...]`：
   - 命中 `@[...]` 时，直接调用 `parse_state_decl_after_at(...)`
   - 其它 `@...` 起始语句继续抛稳定异常并交由外层逐语句 fallback
3. 同步扩展 subset gate：
   - `TOK_AT`
   - `TOK_LBOXBRAC`
   - `TOK_RBOXBRAC`
4. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnSnapshotDeclSubsetSamples`
   - `StyioParserEngine.LegacyAndNewMatchOnM7SnapshotSample`
   - `StyioParserEngine.ShadowArtifactDetailShowsZeroFallbackForSnapshotDeclSubset`

## Alternatives

1. 把所有 `@...` 起始语法一次性全部纳入 new subset：
   - 拒绝。`@file{...} >> ...` 仍与 iterator grammar 强耦合，风险过高。
2. 继续通过入口级 fallback 吃掉 `@[...]`：
   - 拒绝。会让 M7 snapshot 类样例的 route stats 长期偏高。

## Consequences

1. `@[...]` 现在可以直接由 `new` 主路径进入解析。
2. M7 的 `t03_snapshot`、`t06_snapshot_lock`、`t09_snapshot_accum` 都各自减少了 1 条 fallback。
3. 剩余未收口的主战场已经很明确：collection-start iterator（`@file... >>`、`[1,2,3] >>`）。
