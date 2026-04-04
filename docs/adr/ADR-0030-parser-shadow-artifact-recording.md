# ADR-0030: Parser Shadow 差异工件落盘

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E.5 的 shadow compare 已能在差异时返回失败，但缺少“失败后可直接回放”的工件沉淀能力。出现差异时，开发者仍需要手工二次复现并抓取上下文，恢复成本高。

## Decision

1. 新增 CLI 参数：
   - `--parser-shadow-artifact-dir <dir>`
2. 参数约束：
   - 仅允许和 `--parser-shadow-compare` 联用；
   - 单独使用时返回 `CliError`（退出码 6）。
3. 在 shadow compare 阶段写工件：
   - 每次 compare 生成一条 JSONL 记录（包含 `status/file/engine/hash/size/detail`）；
   - 当 `status != match` 时，额外落盘：
     - 原始输入 `.styio`
     - 主引擎 AST 文本
     - 影子引擎 AST 文本（若可用）
     - 影子错误文本（若存在）
4. 新增回归：
   - `StyioParserEngine.ShadowCompareWritesArtifactRecordWhenDirConfigured`
   - `StyioParserEngine.ShadowArtifactDirRequiresShadowCompareFlag`

## Alternatives

1. 只在 mismatch 时打印 stderr，不做文件沉淀。
2. 完全依赖 CI 日志作为差异证据。
3. 将工件写入固定目录（无 CLI 配置）。

## Consequences

1. 差异或异常可直接回放，减少中断恢复成本。
2. 本地/CI 都可收集结构化样本，便于后续最小化与回归模板化。
3. 参数关系更清晰，避免“误传 artifact dir 但未开启 shadow compare”的隐式失效。
