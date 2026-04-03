# ADR 索引

**文档作用：** 记录 Styio 关键架构与工程决策（为什么做、为什么不做其它方案），用于中断后快速恢复决策上下文。

**Last updated:** 2026-04-03

## 约定

1. 文件命名：`ADR-XXXX-<slug>.md`
2. 最小字段：`Status`、`Context`、`Decision`、`Alternatives`、`Consequences`
3. 关键边界（所有权、ABI、诊断格式、兼容策略）必须有 ADR 再落地实现

## 条目

- `ADR-0001-checkpoint-process.md`
- `ADR-0002-jsonl-diagnostics.md`
- `ADR-0003-llvm-ownership-boundary.md`
- `ADR-0004-unicode-facade.md`
- `ADR-0005-runtime-path-compat-and-error-flag.md`
- `ADR-0006-session-reset-and-ast-attach-boundary.md`
- `ADR-0007-expr-node-raii-binop-bincomp.md`
- `ADR-0008-expr-node-raii-cond.md`
- `ADR-0009-expr-node-raii-wave-fallback-selectors.md`
