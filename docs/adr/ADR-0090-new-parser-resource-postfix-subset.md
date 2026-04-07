# ADR-0090: New Parser 接管资源写入/重定向 Postfix 子集

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0089` 引入逐语句 fallback 与 route stats 后，`new` parser 已经可以观测自己到底吃掉了多少语句。但 `expr >> @file{...}` 与 `expr -> @file{...}` 这类资源后缀仍然会触发 `unsupported continuation`，导致：

1. `new` 主路径在 M5 写文件/重定向样例上仍然落回 legacy；
2. route stats 里的 `legacy_fallback_statements` 无法继续下降；
3. 即使语义本身很简单，也无法证明 `new` subset 已真正覆盖资源后缀这类常见表达式尾巴。

## Decision

1. 在 `parse_expr_new_subset(...)` 的 postfix 阶段新增两种受控接管：
   - `expr >> @file{...}` / `expr >> @{...}` -> `ResourceWriteAST`
   - `expr -> @file{...}` / `expr -> @{...}` -> `ResourceRedirectAST`
2. 资源路径仍复用 legacy 已冻结的 `parse_resource_file_atom(context)`，不重复实现 `@file{...}` 细节。
3. `>>` 若后面不是 `@...`，继续抛稳定异常并交由外层逐语句 fallback 处理；不把 iterator tail 一并拉进本切片。
4. 新增回归：
   - `StyioSecurityNewParserStmt.MatchesLegacyOnResourcePostfixSubsetSamples`
   - `StyioParserEngine.LegacyAndNewMatchOnM5WriteFileSample`
   - `StyioParserEngine.LegacyAndNewMatchOnM5RedirectSample`
   - `StyioParserEngine.ShadowArtifactDetailShowsZeroFallbackForResourcePostfixSubset`

## Alternatives

1. 继续完全依赖 legacy fallback：
   - 拒绝。无法继续压缩 `legacy_fallback_statements`，也无法证明 `new` 主路径对资源后缀有真实覆盖。
2. 一次性把 `>>` 的 iterator tail 也全部接管：
   - 拒绝。范围过大，会把流式 iterator 语义和资源写入语义耦合到同一 checkpoint。

## Consequences

1. M5 里的基础资源写入/重定向样例现在可以直接由 `new` subset 处理。
2. route stats 现在能用 `legacy_fallback_statements=0` 直接验证资源后缀语句已不再回退。
3. `>>` 的非资源语义、以及 `@file{...}` 起始的流式输入语句，仍然保留在后续切片中继续收口。
