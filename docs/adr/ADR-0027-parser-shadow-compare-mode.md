# ADR-0027: Parser Shadow Compare 模式

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E.5 需要一个可显式启用的“影子比对模式”，在不改变默认解析行为的前提下，对同一输入做双引擎解析并比较结构结果，尽早暴露 legacy/new 差异。

现有测试已经覆盖若干样例，但缺少一个可在 CLI 层直接打开的统一入口。

## Decision

1. 新增 CLI 开关：`--parser-shadow-compare`（默认 `false`）。
2. 启用后行为：
   - 先按 `--parser-engine` 选择的主引擎完成正常解析；
   - 再用另一引擎做影子解析；
   - 比较 AST `toString` 结果，不一致则报 `ParseError` 并返回稳定非零退出码。
3. 影子解析失败（异常）也视为 `ParseError`，并输出结构化诊断（兼容 `--error-format=jsonl`）。
4. 新增测试：`StyioParserEngine.ShadowCompareAcceptsM1TypedBindSample`。

## Alternatives

1. 仅保留测试内双解析，不提供 CLI 模式。
2. 影子差异仅警告不失败。
3. 始终开启影子比对（无开关）。

## Consequences

1. 保持默认路径零行为变化，影子比对由显式开关触发。
2. 在 CI/本地可快速定位 parser 演进差异，支撑 E.5 差异清单沉淀。
3. 性能开销仅在开启开关时承担。
