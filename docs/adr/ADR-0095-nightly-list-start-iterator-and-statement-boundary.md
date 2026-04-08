# ADR-0095: Nightly Parser 接管 List-Start Iterator 入口与换行边界

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0095: Nightly Parser 接管 List-Start Iterator 入口与换行边界.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0093` 和 `ADR-0094` 之后，M7 的剩余入口级 fallback 基本只集中在 `[ ... ] >> ...` 这一类 list-start iterator。

典型残留样例有两类：

1. 直接以 list 开头的 iterator / zip：
   - `[1, 2, 3] >> #(x) => { ... }`
   - `[1, 2, 3] >> #(n) & [4, 5, 6] >> #(m) => { ... }`
2. 绑定语句后换行再起 list iterator：
   - `result = true`
   - 下一行 `[1, 2, 3] >> #(x) => { ... }`

在旧实现中，nightly expression 子集既不能把 `[` 作为 collection atom 起点，也会把换行后的 `[` 误判为“当前表达式还在继续”，从而触发不必要的 legacy fallback。

## Decision

1. 将共享 list 解析 helper 提升为 `parse_list_exprs_latest_draft(...)`：
   - 仍是 legacy/nightly 共用的稳定逻辑；
   - 因本 checkpoint 正在改造中，暂时保留 `_latest_draft`。
2. 新增 `parse_list_expr_or_iterator_nightly_draft(...)`：
   - 先调用 `parse_list_exprs_latest_draft(...)` 解析 list / infinite collection atom；
   - 若后续 token 是 `ITERATOR`，则复用 `parse_iterator_only_latest(...)`；
   - 否则直接返回 collection expression。
3. nightly expr subset 的 start gate 与 token gate 正式纳入 `[` / `]` / `,` / `...`。
4. nightly expression 解析新增“换行边界优先”规则：
   - 若 `[` 紧随换行出现，则视为下一条 statement 的开头；
   - 只有同一逻辑行上的 `[` 才继续参与 unsupported continuation 判定。
5. 为保持 list-start 暴露后路由稳定，nightly postfix 额外接管 `InfiniteAST` 上的 loop 形式：
   - `?(...) >> ...`
   - `=> ...`

## Alternatives

1. 继续让 `[ ... ] >> ...` 永久依赖逐语句 fallback：
   - 拒绝。M7 的 zip collections / zip unequal 会长期维持非必要回退，route stats 也无法归零。
2. 一次性把完整 zip / forward / cases grammar 全部迁进 nightly：
   - 拒绝。切面过大，不符合当前 checkpoint 粒度。
3. 把换行后的 `[` 继续当作 unsupported continuation：
   - 拒绝。会把“上一条赋值 + 下一条 iterator”误诊为同一表达式残片。

## Consequences

1. M7 shadow gate 现在可以在 `tests/milestones/m7` 上达到 `legacy_fallback_statements=0`。
2. `LegacyAndNightlyMatchOnM7ZipCollectionsSample`、`LegacyAndNightlyMatchOnM7ZipUnequalSample`、`ShadowArtifactDetailShowsZeroFallbackForListIteratorSubset`、`ShadowArtifactDetailShowsZeroFallbackAcrossListBoundaryAfterBind` 成为新的恢复锚点。
3. `parse_list_exprs_latest_draft(...)` 与 `parse_list_expr_or_iterator_nightly_draft(...)` 仍处于 `_draft` 阶段；后续若边界稳定，可再升格为 `_latest` / `_nightly`。
4. E.7 的剩余工作不再是 list-start 入口本身，而是更细粒度的内部 legacy helper 复用面清理。
