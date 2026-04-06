# ADR-0067: Nightly fuzz 失败样本必须生成可回流的 case pack

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

当前 nightly fuzz 仅上传 `fuzz-artifacts/`，包含原始 `crash-*`/`timeout-*` 样本与日志。  
该形态对“快速回归固化”不友好：

1. 样本命名不稳定，跨 run 比对成本高。
2. 缺少统一 manifest 与最小回归模板，恢复时需要人工拼装上下文。
3. 失败样本容易停留在 artifact，不会自动进入可追踪的回归流程。

## Decision

1. 引入 `scripts/fuzz-regression-pack.sh` 作为统一打包入口，输出：
   - `summary.json`
   - `manifest.tsv`
   - 规范化 `corpus-backflow/*.seed`
   - `CASE.md` 与 `apply-corpus-backflow.sh`
2. nightly workflow 在 deep fuzz 后无条件执行打包，并上传 `fuzz-regressions/` 工件。
3. deep fuzz 阶段改为“先收集退出码，再统一失败判定”，确保失败样本仍可打包归档。
4. 新增 `fuzz_regression_pack_smoke` 自动化测试，保证打包链路可执行且可验证。
5. fuzz smoke 运行改为“临时 corpus 副本”模式，避免本地/CI 运行污染 `tests/fuzz/corpus/`。

## Alternatives

1. 保持现状，仅上传原始 artifacts（恢复效率低）。
2. 在 nightly 失败时直接写回仓库 corpus（会引入自动提交与审阅风险）。
3. 仅依赖人工模板记录，不做脚本化归档（过程不可重复）。

## Consequences

1. B.5/D.5 从“上传 artifact”升级为“可复现、可回流、可回归”闭环。
2. 失败 run 的调试入口固定化，减少中断恢复上下文成本。
3. PR/CI 增加一个轻量 smoke 测试，但收益是 nightly 回归链路稳定可控。
