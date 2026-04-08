# ADR-0100: M5 Shadow Gate 采用 Expected-Nonzero Manifest

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0100: M5 Shadow Gate 采用 Expected-Nonzero Manifest.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

`m5` 资源流目录里包含 `t06_fail_fast`。这个样例的正确行为就是运行期报 `RuntimeError` 并非零退出，但 legacy/nightly 的 shadow artifact 仍然一致，而且 route stats 已经是零回退、零 internal bridge。

在现有 `parser-shadow-suite-gate.sh` 中，任何非零退出都会被直接计为失败。这会让 `m5` 无法进入与 `m1/m2/m7` 同级的 shadow gate，即使真正需要守住的 parser 一致性已经满足。

## Decision

1. `scripts/parser-shadow-suite-gate.sh` 支持 suite 目录下的可选清单：
   - `shadow-expected-nonzero.txt`
2. 当样例名出现在该清单里时：
   - 非零退出不再直接算 failed run
   - 但仍必须生成 shadow artifact
   - 且 artifact 仍必须满足 `status=match`
3. 如果 manifest 样例意外返回 `0`，gate 反而失败：
   - 这是 fail-fast 契约漂移，不应被静默接受
4. `tests/milestones/m5/shadow-expected-nonzero.txt` 先登记：
   - `t06_fail_fast`
5. 新增 CTest：
   - `parser_shadow_gate_m5_dual_zero_expected_nonzero`
6. `scripts/checkpoint-health.sh` 默认纳入该 gate。

## Alternatives

1. 把 `t06_fail_fast` 从 `m5` gate 中排除：
   - 拒绝。这样会丢掉一个真实高风险路径的 shadow 守门。
2. 为 `m5` 单独写一份脚本：
   - 拒绝。suite gate 应该保持统一机制，只在数据层声明例外。
3. 允许 manifest 样例无论退出码如何都通过：
   - 拒绝。expected nonzero 也是稳定契约，不能放松成“随便成功或失败都算过”。

## Consequences

1. `m5` 现在可以正式进入 `checkpoint-health`，而不会因为预期中的 fail-fast 样例被误判失败。
2. shadow gate 既保留了 parser 一致性要求，也保留了 runtime fail-fast 契约。
3. 后续若其它目录需要类似例外，只需在各自 suite 目录追加 `shadow-expected-nonzero.txt`，不需要复制脚本逻辑。
