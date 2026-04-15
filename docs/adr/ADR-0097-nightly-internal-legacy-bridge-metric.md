# ADR-0097: Nightly Internal Legacy Bridge Metric 与 Body Helper 收缩

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0097: Nightly Internal Legacy Bridge Metric 与 Body Helper 收缩.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0095` 和 `ADR-0096` 之后，nightly parser 已经能在 `m7` 套件上实现：

1. 顶层 statement route 的 `legacy_fallback_statements=0`
2. shadow artifact 全部 `status=match`

但这仍然不能回答一个关键问题：nightly 内部 helper 是否还在悄悄复用 legacy body parser。

典型残留位置包括：

1. `InfiniteAST` loop body
2. iterator / zip body
3. `?={...}` match-cases body
4. block forward 子句 body

如果不把这类内部 bridge 可视化，E.7 是否能真正移除 `legacy` 主路径就无法量化。

## Decision

1. `StyioParserRouteStats` 新增 `nightly_internal_legacy_bridges`。
2. `StyioContext` 新增 route stats 指针挂载：
   - `set_parser_route_stats_latest(...)`
   - `parser_route_stats_latest()`
   - `note_nightly_internal_legacy_bridge_latest()`
3. nightly shadow 主入口在解析期间挂载 route stats，使内部 helper 可以上报 bridge 次数。
4. 新增 nightly-first body helper：
   - `parse_stmt_subset_with_legacy_fallback_latest_draft(...)`
   - `parse_block_only_subset_with_legacy_fallback_latest_draft(...)`
   - 只有当 nightly subset 失败并回退 legacy 时，才增加 `nightly_internal_legacy_bridges`。
5. nightly 专用 helper 收缩一段内部 legacy 复用面：
   - `parse_cases_only_nightly_draft(...)`
   - `parse_forward_as_list_nightly_draft(...)`
   - `parse_iterator_only_nightly_draft(...)`
   - `parse_infinite_loop_body_nightly_draft(...)`
6. shadow artifact `detail` 追加 `nightly_internal_legacy_bridges=<N>`。

## Alternatives

1. 继续只记录顶层 `legacy_fallback_statements`：
   - 拒绝。它无法反映 nightly 内部仍在借 legacy body parser 的事实。
2. 直接删除内部 legacy helper 调用，不做指标：
   - 拒绝。没有量化指标，无法判断是否真的在持续收敛。
3. 为每个 helper 单独加日志：
   - 拒绝。噪音太大，也不适合作为稳定 gate 字段。

## Consequences

1. `primary_route` 现在同时暴露：
   - `nightly_subset_statements`
   - `legacy_fallback_statements`
   - `nightly_internal_legacy_bridges`
2. `ShadowArtifactDetailShowsZeroFallbackForMixedRouteProgram`、`ShadowArtifactDetailShowsZeroFallbackForListIteratorSubset`、`ShadowArtifactDetailTracksZeroInternalLegacyBridgesForMatchCasesSubset` 现在都冻结 `nightly_internal_legacy_bridges=0`。
3. E.7 后续可以围绕这项指标继续缩小 iterator / forward / cases 的剩余内部 legacy 复用面，而不是只看顶层 route 是否为零。
