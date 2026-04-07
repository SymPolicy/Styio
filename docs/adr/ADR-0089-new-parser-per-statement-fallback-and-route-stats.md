# ADR-0089: New Parser 主入口改为逐语句 Fallback，并记录 Route Stats

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0087` / `ADR-0088` 之后，`new` parser 已能原生处理更多语法片段，但 `parse_main_block_new_shadow(...)` 仍然使用“整文件 token gate”策略：

1. 只要文件中出现任一非子集 token，整个程序都会直接回退到 legacy；
2. 默认 `new` 主路径即使在混合程序里只靠少量 unsupported 语句触发回退，也无法保留已接管语句的真实流量；
3. shadow artifact 只能记录 `primary_engine` / `shadow_engine`，无法观测 `new` 主路径内部到底有多少语句走了 subset、多少语句落回 legacy。

## Decision

1. 将 `parse_main_block_new_shadow(...)` 改为逐语句路由：
   - 当前语句满足 subset 起点时，先尝试 `parse_stmt_new_subset(...)`；
   - 若失败，则仅回退当前语句到 legacy `parse_stmt_or_expr(...)`；
   - 不再因为单条 unsupported 语句让整文件直接切回 legacy。
2. 新增 `StyioParserRouteStats`，记录：
   - `new_subset_statements`
   - `legacy_fallback_statements`
3. 在 shadow artifact `detail` 中写入 route stats：
   - 例如 `primary_route=new_subset_statements=1,legacy_fallback_statements=2`
4. 为避免 name-led 语句被 new expr 子集“吃半截”：
   - 在 `parse_expr_new_subset(...)` 后检测 unsupported continuation（如 `<-` / `>>` / `>` / `[` / `|` / `<<`）；
   - 命中时抛异常，由外层逐语句 fallback 接管 legacy 路径。
5. 新增回归：
   - `StyioParserEngine.ShadowArtifactDetailCapturesPerStatementFallbackStats`

## Alternatives

1. 继续保持整文件 token gate：
   - 拒绝。会长期掩盖 E.7 的真实接管进度。
2. 直接移除 fallback，强制所有语句都由 new parser 处理：
   - 拒绝。当前仍处于 Strangler 阶段，风险过高。

## Consequences

1. 混合程序里，已接管语句可以继续走 `new`，unsupported 语句仅局部回退。
2. shadow artifact 现在能直接反映 `new` 主路径内部的 route 结构，恢复与巡检成本降低。
3. 当前 route stats 仍是入口级观测，不代表 block/cases/iterator 内部已完全摆脱 legacy。
