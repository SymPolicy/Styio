# ADR-0096: M7 Shadow Zero-Fallback Gate 进入 CTest 与 Checkpoint Health

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0096: M7 Shadow Zero-Fallback Gate 进入 CTest 与 Checkpoint Health.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0095` 后，`tests/milestones/m7` 已经可以通过 nightly shadow compare 达到：

1. shadow artifact 全部 `status=match`
2. `legacy_fallback_statements=0`

但这个状态此前仍依赖手工命令复核，尚未进入常规 CTest 或 `scripts/checkpoint-health.sh`。这会带来两个问题：

1. 后续 parser 收缩若让 M7 回退重新升高，恢复脚本不会第一时间报警；
2. “零 fallback” 仍是口头状态，不是工程门槛。

## Decision

1. `scripts/parser-shadow-suite-gate.sh` 新增 `--require-zero-fallback` 选项：
   - 保留原有 match/mismatch/shadow_error 统计；
   - 额外统计 `nonzero_fallback_records`；
   - 在启用该选项时，任何 `legacy_fallback_statements=[1-9]` 都直接使 gate 失败。
2. 在 `tests/CMakeLists.txt` 新增 `parser_shadow_gate_m7_zero_fallback`：
   - 目标目录固定为 `tests/milestones/m7`；
   - 标签为 `styio_pipeline;shadow_gate;m7`。
3. `scripts/checkpoint-health.sh` 将该 gate 纳入默认非 ASan 路径。

## Alternatives

1. 继续只靠单元测试中的 shadow artifact 断言：
   - 拒绝。单测覆盖的是代表性样例，不是整组 `m7` 样例集。
2. 只在 nightly CI 工作流里手工执行脚本：
   - 拒绝。本地恢复与 CI 会漂移，违背 checkpoint 机制。
3. 为每个样例单独新增一个 zero-fallback 单测：
   - 拒绝。维护成本高，且不如直接复用 suite gate。

## Consequences

1. `./scripts/checkpoint-health.sh --no-asan` 现在会显式守住 M7 的 nightly 零 fallback 状态。
2. parser 收缩后若让 M7 任一样例重新出现 `legacy_fallback_statements>0`，CTest 与恢复脚本都会直接失败。
3. 后续若 M1/M2/M5 也需要同级别“零 fallback”门槛，可复用同一个脚本选项，不必另起实现。
