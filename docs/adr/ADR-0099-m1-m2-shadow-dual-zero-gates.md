# ADR-0099: M1/M2 Shadow Dual-Zero Gates 进入 Checkpoint Health

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0098` 后，`m7` 已经被提升为 `zero-fallback + zero-internal-bridges` 双门槛。但 `m1` 与 `m2` 虽然已经实测满足同样条件，当前仍只是“人工确认通过”，还没有进入 CTest 或恢复脚本。

这会留下两个问题：

1. `legacy` 退场前，最基础的表达式/函数语法域没有被纳入同级别稳定窗口；
2. 后续 parser 切片若让 `m1/m2` 重新出现 fallback 或 internal bridge，`checkpoint-health` 不会第一时间报警。

## Decision

1. 在 `tests/CMakeLists.txt` 新增两个双零门禁：
   - `parser_shadow_gate_m1_zero_fallback_and_internal_bridges`
   - `parser_shadow_gate_m2_zero_fallback_and_internal_bridges`
2. 两个 gate 都执行：
   - `scripts/parser-shadow-suite-gate.sh --require-zero-fallback --require-zero-internal-bridges`
3. `scripts/checkpoint-health.sh` 默认纳入 `m1/m2` 双零门禁，位置放在 `styio_pipeline/security` 之后、`m7` 专项门禁之前。
4. `m7` 继续保留拆开的两条门禁：
   - `parser_shadow_gate_m7_zero_fallback`
   - `parser_shadow_gate_m7_zero_internal_bridges`

## Alternatives

1. 继续只守 `m7`：
   - 拒绝。`m7` 是高风险语法聚合区，但不能代替 `m1/m2` 的基础语法稳定窗口。
2. 为 `m1/m2` 分别拆成四条 gate：
   - 拒绝。当前 `m1/m2` 结构简单，合并成双零 gate 成本更低，表达也更直接。
3. 只把它们写进文档，不进恢复脚本：
   - 拒绝。恢复脚本必须代表当前真实门槛。

## Consequences

1. `./scripts/checkpoint-health.sh --no-asan` 现在同时守住：
   - `m1` 双零
   - `m2` 双零
   - `m7` 零 fallback
   - `m7` 零 internal bridge
2. `legacy` 主路径移除前，基础语法域和高风险聚合语法域都已有 shadow 双轨硬门槛。
3. 后续若要把 `m5` 提升到同级别门槛，需要先处理其 fail-fast 样例与 suite gate 的兼容方式。
