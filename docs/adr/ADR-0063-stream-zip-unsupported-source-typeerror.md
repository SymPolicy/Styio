# ADR-0063: Stream Zip 不支持来源的错误分类收敛为 TypeError

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

`SGStreamZip` 在 codegen 仅支持部分来源组合（list literal / `@file`）。  
当输入使用“非字面量列表变量 + list literal”组合时，例如：

```styio
a = [1, 2, 3]
a >> #(n) & ["x", "y", "z"] >> #(s) => { >_(n + " " + s) }
```

会落到 `throw StyioNotImplemented("unsupported stream zip lowering")`。  
该异常在执行阶段被顶层归类为 `RuntimeError`（退出码 `5`），但这本质上是用户输入语义边界问题，不是运行期故障。

## Decision

1. 将 `SGStreamZip` 默认兜底分支从 `StyioNotImplemented` 改为 `StyioTypeError`。
2. 错误文案明确当前支持边界：
   - `unsupported stream zip lowering (supported sources: list literal and @file stream)`
3. 新增红灯-转绿回归：
   - `StyioDiagnostics.StreamZipUnsupportedSourceReportsTypeError`

## Alternatives

1. 保持 `StyioNotImplemented` 并继续归类 `RuntimeError`。
2. 在 `main` 对 `StyioNotImplemented` 做全局改映射。
3. 在 parser/analyzer 阶段前置拒绝，不在 codegen 检查。

## Consequences

1. 用户在该路径稳定收到 `TypeError`（退出码 `4`），错误分类与语义性质一致。
2. JSONL 诊断契约更稳定，减少 IDE/LSP 侧分类歧义。
3. 保留 codegen 兜底，后续若扩展 zip 来源组合，只需放开该分支并补相应回归。
