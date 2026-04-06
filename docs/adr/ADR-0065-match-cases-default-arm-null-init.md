# ADR-0065: `?=` 无默认分支时 `CasesAST` 默认臂指针必须显式置空

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

`parse_cases_only` 在解析 `?= { ... }` 时，局部变量 `default_stmt` 未初始化。  
当输入没有 `_ => ...` 默认分支时，会把未定义指针传入 `CasesAST::Create(...)`，后续在分析/生成阶段触发随机崩溃（已复现 `SIGBUS` / `SIGSEGV`）：

```styio
x = 1
x ?= {
  1 => >_(1)
}
```

该问题属于 parser 内存安全边界缺陷，不应由运行时行为“碰运气”决定是否崩溃。

## Decision

1. 在 `parse_cases_only` 中将 `default_stmt` 显式初始化为 `nullptr`。
2. 保持既有语义：`?=` 默认分支可选；缺失时 `CasesAST::case_default == nullptr`。
3. 增加稳定回归：
   - `StyioDiagnostics.MatchWithoutDefaultDoesNotCrash`
   - 断言退出码 `0` 且输出为 `1\n`（匹配分支命中时）。

## Alternatives

1. 强制 `?=` 必须包含 `_` 默认分支（语法层拒绝）。
2. 在 analyzer/codegen 兜底忽略悬空默认臂（治标不治本）。
3. 将缺失默认统一改报 `TypeError`（语义变化更大，需额外迁移评估）。

## Consequences

1. “无默认分支 match”路径从崩溃转为稳定可执行。
2. parser 侧去除未定义行为，减少 fuzz/soak 下非确定性失败。
3. 后续若要收紧语义（例如强制默认分支），可在 `nullptr` 基线之上演进，不影响当前稳定性修复。
