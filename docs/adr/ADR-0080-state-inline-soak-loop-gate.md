# ADR-0080: State helper 内联链路 Soak Gate

- Status: Accepted
- Date: 2026-04-06

## Context

在修复单参数 state helper（直返 `StateDecl`）内联识别后，单次单元回归已能覆盖功能正确性，但仍缺少长生命周期运行中的稳定性门禁：

1. 连续多轮调用是否保持输出稳定；
2. `soak_deep` 是否覆盖该链路，避免夜间回归盲区。

## Decision

1. 在 `tests/soak/styio_soak_test.cpp` 新增：
   - `StyioSoakSingleThread.StateInlineHelperProgramLoop`
   - 每轮执行内联样例并断言输出稳定为 `1/3/6`。

2. 在 `tests/CMakeLists.txt` 新增 deep 档位用例：
   - `soak_deep_state_inline_program`
   - 环境变量：`STYIO_SOAK_STATE_INLINE_ITERS=1500`。

3. 在 `tests/soak/README.md` 补齐说明与环境变量，保证中断后可恢复执行。

## Alternatives

1. 仅依赖 `StyioDiagnostics.SingleArgStateFunctionInliningUsesCallArgument`：
   - 被拒绝。单次功能测试不覆盖长跑稳定性。

2. 把样例塞到现有 `StreamProgramLoop`：
   - 被拒绝。用例语义不同，失败定位会变差。

## Consequences

1. `soak_smoke` 覆盖面从 6 条提升到 7 条，新增 state helper 内联长跑回归。
2. `soak_deep` 新增独立 gate，可在 nightly 直接暴露该链路回归。
3. 中断恢复时可以直接通过 `ctest -R '^soak_deep_state_inline_program$'` 快速定位此链路健康状态。
