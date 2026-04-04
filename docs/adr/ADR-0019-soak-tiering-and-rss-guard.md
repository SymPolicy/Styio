# ADR-0019: Soak 分档与 RSS 增长守卫

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

D 阶段需要同时满足两件事：

1. PR 快速反馈（不能把深跑压力塞进主 CI）
2. 长生命周期稳定性验证（必须有可持续深跑入口）

仅有单一 `soak_smoke` 档位不够，无法在不拖慢 PR 的前提下持续放大 `open/read/rewind/close` 与流式程序重放压力。

## Decision

1. 保留 `soak_smoke` 作为 PR 默认档位。
2. 在 `tests/CMakeLists.txt` 增加 `soak_deep_*` 测试条目，并按用例注入放大量 `STYIO_SOAK_*` 环境变量。
3. 新增 `StyioSoakSingleThread.FileHandleMemoryGrowthBound`，通过 RSS 增长阈值做回归门槛。
4. 新增 nightly 工作流 `.github/workflows/nightly-soak.yml`，执行 `ctest -L soak_deep` 并归档日志。

## Alternatives

1. 仅依赖 ASan/LSan，不做 RSS 增长阈值。
2. 所有 soak 用例统一使用一档参数（无 smoke/deep 分离）。
3. 只本地手工跑 deep，不提供 nightly 自动化。

## Consequences

1. PR 时长保持可控，同时 nightly 获得稳定深跑覆盖。
2. 句柄生命周期回归可通过 RSS 阈值更早暴露。
3. D.5 的失败样本最小化可直接基于 nightly 日志落地。
