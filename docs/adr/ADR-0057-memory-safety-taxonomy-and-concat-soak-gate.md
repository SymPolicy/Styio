# ADR-0057: 内存安全（Safety）术语收敛与拼接 Soak 门禁

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0057: 内存安全（Safety）术语收敛与拼接 Soak 门禁.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

`styio_strcat_ab` 的生命周期收口进入 C.7.2 后，需要稳定的长跑回归来验证“已释放路径”在高频链式拼接下不会出现明显内存增长回归。  
同时，现有测试命名中把运行时内存生命周期问题放在 `SecurityRuntime` 语义下，容易与攻防语义混淆。

## Decision

1. 将运行时内存/资源生命周期测试命名收敛到 `StyioSafetyRuntime.*`。
2. 保留 `security` 测试标签用于历史兼容分组，但文档和用例命名明确“内存安全 = Safety”。
3. 新增 soak 用例 `StyioSoakSingleThread.ConcatMemoryGrowthBound`：
   - 高频 `styio_strcat_ab` + `styio_free_cstr` 链式循环；
   - RSS 增长阈值断言。
4. 在 deep soak 增加 `soak_deep_concat_rss_guard` 夜间门禁，并注入放大量参数。

## Alternatives

1. 不改术语，继续沿用 `SecurityRuntime`。
2. 仅做单元测试，不引入内存增长 Soak 门禁。
3. 直接在 PR 流水线上启用深度 concat 压测。

## Consequences

1. 术语一致性提升：运行时内存问题的讨论边界更清晰（Safety vs Security）。
2. `styio_strcat_ab` 的回归检测从“功能正确”扩展到“长期内存曲线约束”。
3. 通过 nightly deep soak 控制时长成本，避免 PR 流水线被重压测拖慢。
4. `safety` 作为当前测试可见标签按套件级传播；后续若需要更细粒度（仅运行时生命周期用例）再拆分独立测试二进制。
