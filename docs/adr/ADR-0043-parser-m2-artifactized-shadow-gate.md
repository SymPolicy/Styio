# ADR-0043: M2 Shadow Gate 脚本化并产出工件

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

M2 gate 已升级到 full suite，但仍依赖单条 `ctest` 断言。相比 M1（脚本化预算 + `summary.json` + 失败工件），M2 在失败定位与统计口径上不一致。

## Decision

1. CI 的 M2 gate 切换为脚本化执行：
   - `scripts/parser-shadow-m1-gate.sh ./build/bin/styio ./tests/milestones/m2 ...`
2. 产物策略与 M1 对齐：
   - 生成 M2 `summary.json`；
   - 失败时上传 M2 工件目录。
3. 上传步骤合并打包：
   - `build/parser-shadow-artifacts`（M1）
   - `build/parser-shadow-artifacts-m2`（M2）

## Alternatives

1. 维持 M2 为 `ctest` gate，不产出预算工件。
2. 单独写一套 M2 专用脚本。
3. 只产出日志，不上传工件目录。

## Consequences

1. M1/M2 gate 统计与工件格式统一，便于稳定窗口分析。
2. M2 失败可直接下载样本回放，排障更快。
