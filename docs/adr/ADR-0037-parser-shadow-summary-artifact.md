# ADR-0037: Parser Shadow Gate 增加 Summary 工件

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

`parser-shadow-suite-gate.sh` 已有终端统计输出与逐样例 JSONL 工件，但缺少统一机器可读汇总文件。后续若要做稳定窗口统计或跨构建聚合，需要再解析日志，成本较高。

## Decision

1. `scripts/parser-shadow-suite-gate.sh` 在 artifact 根目录额外写入 `summary.json`。
2. 汇总字段固定：
   - `cases/runs/artifacts/match/mismatch/shadow_error/failed_runs/passed`
3. 失败判定规则保持不变，仅新增可消费摘要。

## Alternatives

1. 继续只输出终端日志，不产出 summary。
2. 由 CI 另写脚本解析日志生成 summary。
3. 在每个 case 目录分散写状态，不生成全局汇总。

## Consequences

1. 后续稳定窗口统计可直接读取 `summary.json`。
2. 工件消费接口更稳定，不依赖日志格式。
3. 增加一个轻量输出文件，维护成本低。
