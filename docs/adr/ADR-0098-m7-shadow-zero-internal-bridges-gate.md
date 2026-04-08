# ADR-0098: M7 Shadow Zero-Internal-Bridges Gate 与 Plain State Ref 子集补齐

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0098: M7 Shadow Zero-Internal-Bridges Gate 与 Plain State Ref 子集补齐.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0097` 后，shadow artifact 已经能量化 `nightly_internal_legacy_bridges=<N>`，但这项指标还只是“可观测”，不是自动化门槛。

对 `tests/milestones/m7` 的整组扫描显示，剩余 internal bridge 主要集中在 snapshot 系列样例：

1. `t03_snapshot`
2. `t06_snapshot_lock`
3. `t09_snapshot_accum`

根因不是 statement route 回退，而是 nightly expr 子集尚未接管 plain state ref `$name`。这些样例在 iterator body 中出现 `$ref`、`$factor` 之类表达式时，会触发 nightly 内部对 legacy expr/body helper 的一次 bridge。

如果不把这部分补齐并升格为 gate，`m7` 虽然可以继续维持顶层 `legacy_fallback_statements=0`，但 nightly 内部仍可能重新借回 legacy helper，而恢复脚本不会报警。

## Decision

1. nightly expr 子集新增 plain state ref `$name`：
   - 新增 `parse_state_ref_nightly_draft(...)`
   - `parse_primary(...)` 接入 `TOK_DOLLAR`
   - `styio_parser_expr_subset_token_nightly(...)` 与 `styio_parser_expr_subset_start_nightly(...)` 同步纳入 `TOK_DOLLAR`
2. `scripts/parser-shadow-suite-gate.sh` 新增 `--require-zero-internal-bridges`：
   - summary 新增 `nonzero_internal_bridge_records`
   - 启用后，任意 artifact 出现 `nightly_internal_legacy_bridges=[1-9]` 即 gate 失败
3. `tests/CMakeLists.txt` 新增 `parser_shadow_gate_m7_zero_internal_bridges`
4. `scripts/checkpoint-health.sh` 默认纳入该 gate，使本地恢复与 CTest 保持一致
5. `StyioParserEngine.ShadowArtifactDetailShowsZeroFallbackForSnapshotDeclSubset` 冻结 `nightly_internal_legacy_bridges=0`，把 snapshot 系列变成明确书签

## Alternatives

1. 只补 `$name` state ref，不新增 gate：
   - 拒绝。没有 gate，这个状态很容易在后续切片里悄悄回退。
2. 把 internal bridge 只写进 history 文档：
   - 拒绝。文档不能替代自动化门槛。
3. 直接要求所有 milestone 套件都零 internal bridge：
   - 暂不采用。当前先把 `m7` 作为高风险 grammar 聚合区收紧，再决定是否向 `m1/m2/m5` 扩展。

## Consequences

1. `tests/milestones/m7` 现在同时受两条硬门槛约束：
   - `legacy_fallback_statements=0`
   - `nightly_internal_legacy_bridges=0`
2. `./scripts/checkpoint-health.sh --no-asan` 现在会在 parser 收敛回退时更早失败。
3. E.7 后续可以把“移除 legacy 主路径”的判断依据从单一 zero-fallback，升级为“zero-fallback + zero-internal-bridges”双条件。
