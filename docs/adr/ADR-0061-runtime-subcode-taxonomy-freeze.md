# ADR-0061: Runtime 子码（subcode）稳定化与覆盖冻结

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0061: Runtime 子码（subcode）稳定化与覆盖冻结.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

ADR-0060 已统一 runtime 错误由顶层输出 text/jsonl，但 runtime 子类错误仍需要稳定子码，便于：

1. 测试做精确断言（不仅断言 `STYIO_RUNTIME` 主码）；
2. IDE/LSP 后续按子码分流提示；
3. 在 first-error-wins 语义下保留首个根因分类。

## Decision

1. 冻结当前 runtime 子码集合：
   - `STYIO_RUNTIME_INVALID_FILE_HANDLE`
   - `STYIO_RUNTIME_FILE_PATH_NULL`
   - `STYIO_RUNTIME_FILE_OPEN_READ`
   - `STYIO_RUNTIME_FILE_OPEN_WRITE`
2. 在 runtime helper 内使用集中常量定义子码，避免字符串漂移。
3. 保持主码不变：所有 runtime 错误仍对外归类为 `STYIO_RUNTIME`，子码只做细分。
4. first-error-wins 同时约束消息与子码：同一执行周期中后续错误不得覆盖首个子码。
5. 为 read/write/path null/invalid handle 与 first-error-wins 补回归测试。

## Alternatives

1. 不引入子码，仅靠 message 文本区分。
2. 每个 runtime helper 自由拼写子码（无集中常量）。
3. 后续错误覆盖首个子码，始终保留“最后一个错误”。

## Consequences

1. `--error-format=jsonl` 的 runtime 诊断可稳定机读细分。
2. 回归测试对 runtime 失败原因有更强约束，降低文案漂移风险。
3. 后续新增 runtime 子类错误需要同步更新 ADR/测试，维护成本小幅上升但可控。
