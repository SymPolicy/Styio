# ADR-0087: New Parser Block/Control 子集接管 Hash Block Body

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0087: New Parser Block/Control 子集接管 Hash Block Body.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

E.6 已将 CLI 默认 parser 切到 `new`，但 `parse_hash_stmt_new_subset(...)` 在遇到 `# ... => { ... }` 时，仍直接调用 legacy `parse_block_with_forward(context)`。

这导致：

1. `new` 主路径在函数 block body 上仍依赖 legacy block 解析；
2. `parse_main_block_new_subset(...)` 对 standalone block / `<<` / `^` / `>>` / `...` 这类控制语句仍会抛 `StyioSyntaxError("unexpected statement token in new parser subset")`；
3. M2 block body / nested function 等样例虽然在 engine 层通过，但实际只是 `new` 外壳 + legacy block body。

## Decision

1. 在 `NewParserExpr.cpp` 扩展 statement 子集起始与 token gate，纳入：
   - `{ ... }` block
   - `<<` return
   - `^` break
   - `>>` continue
   - `...` pass
   - `|>` yield-return
2. 新增 `parse_block_only_new_subset(...)` 与 `parse_block_with_forward_new_subset(...)`：
   - block 内语句优先复用 `parse_stmt_new_subset(...)`；
   - block 后的 forward 列表暂时仍调用 legacy `parse_forward_as_list(...)`。
3. 将 hash `=> { ... }` body 从 legacy `parse_block_with_forward(...)` 切到 `parse_block_with_forward_new_subset(...)`。
4. 新增 security 回归冻结：
   - `StyioSecurityNewParserStmt.SubsetStartGateIncludesBlockAndControlStarters`
   - `StyioSecurityNewParserStmt.MatchesLegacyOnBlockControlSubsetSamples`

## Alternatives

1. 保持 legacy block body，不触碰 new block 子集：
   - 拒绝。默认引擎已切到 `new`，继续让高频 block body 隐式回退会拖慢 E.7 清退。
2. 一次性迁移 block + forward + match/cases + loop 体：
   - 拒绝。范围过大，无法满足 checkpoint 的 1-3 天可合并粒度。

## Consequences

1. `new` parser 在 block/control 语句上的覆盖面向前推进一段，`# ... => { ... }` 不再默认依赖 legacy block parser。
2. 当前 slice 仍保留一条明确边界：
   - block 尾随 forward 列表继续走 legacy `parse_forward_as_list(...)`；
   - 更复杂的 `@` / loop / match-cases block 体仍可能触发保守回退。
3. 中断恢复时可直接通过 security 子集回归定位此边界是否漂移。
