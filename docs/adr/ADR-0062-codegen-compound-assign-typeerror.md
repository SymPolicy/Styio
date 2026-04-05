# ADR-0062: CodeGen 期“不可变变量复合赋值”错误分类改为 TypeError

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

当输入包含：

```styio
x : i64 := 1
x += 2
```

codegen 原实现抛 `StyioNotImplemented("compound assignment requires a mutable binding")`。  
该异常在 `main` 的执行阶段被映射为 `RuntimeError`（退出码 `5`），导致：

1. 用户输入语义错误被误分类为运行期错误；
2. `--error-format=jsonl` 的分类稳定性下降；
3. 与 analyzer 阶段同类错误（`TypeError`）不一致。

## Decision

1. 将 `SGBinOp` 复合赋值路径中“目标非 mutable”错误改为抛 `StyioTypeError`。
2. 同步将 codegen 中两个“immutable re-define”旧 `StyioNotImplemented` 分支改为 `StyioTypeError`，减少同类漂移风险。
3. 新增红灯-转绿回归：
   - `StyioDiagnostics.CompoundAssignOnImmutableBindingReportsTypeError`。

## Alternatives

1. 保持 `StyioNotImplemented`，继续映射 `RuntimeError`。
2. 在 `main` 中特判 `StyioNotImplemented` 映射到 `TypeError`。
3. 将该检查前移到 analyzer，仅保留 codegen 兜底。

## Consequences

1. 该输入路径稳定输出 `TypeError`（退出码 `4`），语义分类更准确。
2. 保留 codegen 兜底，不依赖 analyzer 覆盖完整性。
3. 后续若新增 codegen 语义边界，优先使用 `StyioTypeError`/`StyioParseError`，避免落入 `RuntimeError`。
