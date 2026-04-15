# ADR-0060: Runtime Last-Error API 与顶层诊断统一输出

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0060: Runtime Last-Error API 与顶层诊断统一输出.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

runtime C ABI 在 C.8 已改为“返回错误并由上层处理”，但错误文案仍由 runtime helper 直接写 `stderr`。  
这会导致：

1. `--error-format=jsonl` 下 runtime 失败不是稳定 JSONL；
2. 顶层诊断与 runtime 文案分散，测试和 IDE 集成难以对齐。

## Decision

1. 为 runtime 增加 `styio_runtime_last_error()`（线程本地借用字符串）。
2. runtime helper 错误改为“设置错误状态与消息”，不再直接 `fprintf(stderr, ...)`。
3. `main` 在 `generator.execute()` 后检测到 runtime error 时，统一调用 `styio_emit_diagnostic(...)`：
   - text: `[RuntimeError] <message>`
   - jsonl: `{"category":"RuntimeError","code":"STYIO_RUNTIME",...}`
4. `styio_runtime_clear_error()` 同时清除错误标记与 last-error 消息。
5. runtime error 消息采用 “first error wins” 策略（同一执行周期内保留首个根因）。

## Alternatives

1. 保持 runtime 直接打印文本，main 只返回退出码。
2. 在 runtime 内直接生成 JSONL（引入 CLI 格式耦合）。
3. 每次错误都覆盖 last-error（可能丢失首个根因）。

## Consequences

1. `--error-format=jsonl` 对 runtime 失败也具备稳定机器可读输出。
2. 诊断格式由顶层统一，runtime 与 CLI 边界更清晰。
3. 通过 last-error API，可在单元测试中稳定断言 runtime 失败原因。
