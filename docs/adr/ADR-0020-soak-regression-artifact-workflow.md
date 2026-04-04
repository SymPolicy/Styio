# ADR-0020: Soak 失败样本最小化与回归工单格式

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

D.5 需要把 nightly deep soak 的失败样本稳定沉淀为可重放、可追踪的回归资产。
如果只保留 CI 日志，后续修复常出现两类问题：

1. 无法快速还原最小失败条件
2. 修复完成后缺少统一“已回归”记录格式

## Decision

1. 增加脚本 `scripts/soak-minimize.sh`，对指定 `STYIO_SOAK_*` 变量做二分最小化。
2. 最小化产物统一落在 `tests/soak/regressions/<timestamp>-<case>/`。
3. 产物目录固定包含 `repro.sh`、`env.txt`、`*.log`、`CASE.md`。
4. 提供模板 `tests/soak/REGRESSION-TEMPLATE.md`，统一记录症状、复现、根因、修复与回归覆盖。

## Alternatives

1. 仅保留 nightly workflow 原始日志，不做最小化。
2. 由开发者手工写回归文档，不统一模板。
3. 失败后直接写单元测试，不保留中间样本与环境快照。

## Consequences

1. 中断恢复时可直接从 `repro.sh` + `env.txt` 继续，不依赖记忆。
2. 回归资产结构统一，便于审计与后续自动化。
3. 二分最小化假设“单变量单调”，不满足时需要人工补充说明。
