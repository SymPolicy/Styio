# ADR-0041: Parser M2 Shadow Gate 升级为 Full Suite

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

M2 core gate 已上线，但覆盖范围仍是抽样。随着函数定义入口已稳定接入 NewParser，M2 全量样例 (`t*.styio`) 已可稳定通过 shadow compare。

## Decision

1. 新增 `StyioParserEngine.ShadowCompareAcceptsM2FullSuite`。
2. CI 显式 gate 从 M2 core 升级为 M2 full：
   - `ctest --test-dir build -R '^StyioParserEngine.ShadowCompareAcceptsM2FullSuite$'`
3. 保留 M1 full gate 与 artifact 流程不变。

## Alternatives

1. 继续停留在 M2 core gate。
2. 将 M2 full 并入 M1 gate 脚本，不单独 gate。
3. 等函数语义完全迁移后再升级 gate。

## Consequences

1. M2 回归检测从抽样提升到全量。
2. 默认切换证据链更完整（M1 full + M2 full）。
