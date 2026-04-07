# ADR-0083: State Inline MatchCases Soak Gate

- Status: Accepted
- Date: 2026-04-07

## Context

`ADR-0082` 已修复 state helper 内联在 `MatchCases/Cases` 下的克隆缺口，并通过单元回归验证功能正确性。  
但该路径仍缺少长生命周期守门，nightly 深跑无法覆盖该语法形态，存在回归盲区。

## Decision

1. 在 `tests/soak/styio_soak_test.cpp` 增加：
   - `StyioSoakSingleThread.StateInlineMatchCasesProgramLoop`
   - 循环执行 `?= { ... }` 更新表达式样例并断言输出稳定 `10/12/15`。
2. 在 `tests/CMakeLists.txt` 增加 deep 档位：
   - `soak_deep_state_inline_matchcases_program`
   - 注入环境变量 `STYIO_SOAK_STATE_MATCH_ITERS=1500`。
3. 在 `tests/soak/README.md` 补齐用例与环境变量说明，确保中断恢复时可直接复现。

## Alternatives

1. 仅依赖 `StyioDiagnostics.StateInlineMatchCasesFunctionUsesCallArgument`：
   - 拒绝。单次执行不覆盖长跑稳定性。
2. 并入现有 `StateInlineHelperProgramLoop`：
   - 拒绝。场景语义不同，失败定位会混淆。

## Consequences

1. `soak_smoke` 覆盖从 7 条提升到 8 条。
2. `soak_deep` 新增独立 gate，可在 nightly 单独定位 `MatchCases` 内联回归。
3. 中断恢复时可直接通过 `ctest -R soak_deep_state_inline_matchcases_program` 快速核验此链路。
