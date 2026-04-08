# ADR-0104: Parser Legacy Entry Audit Gate

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0101` 和 `ADR-0103` 之后，five-layer pipeline、checkpoint health 和 parser fuzz 都已经对齐到 `nightly` 或 nightly-first。但这类约束如果只靠“记住不要这么做”，很容易在后续切片里回退：

1. `src/StyioTesting/` 里的 harness 可能再次直接连回 `parse_main_block_legacy(...)`
2. 生产路径也可能绕过 `parse_main_block_with_engine_latest(...)`，重新把 legacy 入口硬编码进来

这类回退不会马上破坏语义，却会让 `E.7` 的退场窗口重新变脏。

## Decision

1. 新增审计脚本：
   - `scripts/parser-legacy-entry-audit.sh`
2. 审计规则固定为：
   - `src/` 中不允许出现 parser core 之外的 `parse_main_block_legacy(...)` 直接调用
   - `src/StyioTesting/` 不允许出现 `StyioParserEngine::Legacy` 或 `parse_main_block_legacy(...)`
   - `src/main.cpp` 必须继续通过 `parse_main_block_with_engine_latest(...)` 进入 parser
3. 该审计进入 CTest：
   - `parser_legacy_entry_audit`
4. `scripts/checkpoint-health.sh` 默认纳入该 gate。

## Alternatives

1. 继续只靠文档和 code review 约束：
   - 拒绝。回退成本太低，且容易在中断恢复时漏看。
2. 直接禁止整个仓库出现 `StyioParserEngine::Legacy`：
   - 拒绝。shadow compare、parity tests 和 dual-engine fuzz 仍然需要合法使用 legacy。
3. 等真正删除 legacy 主路径时再处理：
   - 拒绝。那时再发现验证入口漂移，代价更高。

## Consequences

1. 任何把生产或 `src/StyioTesting` 路径偷偷连回 legacy 的改动，都会先在 CTest/恢复脚本里红灯。
2. `legacy` 的合法暴露面被显式收敛到 parser core、CLI engine selector、shadow/parity 测试和 dual-engine fuzz。
3. 后续继续推进 `E.7` 时，可以用自动化而不是人工记忆来维持退场边界。
