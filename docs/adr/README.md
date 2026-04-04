# ADR 索引

**文档作用：** 记录 Styio 关键架构与工程决策（为什么做、为什么不做其它方案），用于中断后快速恢复决策上下文。

**Last updated:** 2026-04-04

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
- `ADR-0010-expr-node-raii-func-call.md`
- `ADR-0011-expr-node-raii-listop-attr.md`
- `ADR-0012-expr-node-raii-fmtstr-typeconvert-range-sizeof.md`
- `ADR-0013-expr-node-raii-tuple-list-set.md`
- `ADR-0014-stmt-node-raii-return-bind.md`
- `ADR-0015-stmt-node-raii-resource-io.md`
- `ADR-0016-stmt-node-raii-var-family.md`
- `ADR-0017-stmt-node-raii-struct-resource.md`
- `ADR-0018-soak-single-thread-framework.md`
- `ADR-0019-soak-tiering-and-rss-guard.md`
- `ADR-0020-soak-regression-artifact-workflow.md`
- `ADR-0021-parser-engine-shadow-routing.md`
- `ADR-0022-parser-lookahead-shared-utils.md`
- `ADR-0023-parser-expr-subset-shadow-guard.md`
- `ADR-0024-parser-stmt-subset-print-shadow.md`
- `ADR-0025-parser-stmt-subset-flexbind-and-call-fallback.md`
- `ADR-0026-parser-final-bind-subset-and-safe-fallback.md`
- `ADR-0027-parser-shadow-compare-mode.md`
- `ADR-0028-parser-compound-assign-subset.md`
- `ADR-0029-parser-shadow-full-m1-and-bool-literal-alignment.md`
- `ADR-0030-parser-shadow-artifact-recording.md`
- `ADR-0031-parser-default-switch-gate.md`
- `ADR-0032-parser-shadow-budget-script.md`
- `ADR-0033-parser-compare-logic-subset-alignment.md`
- `ADR-0034-refactor-template-shadow-gate-pattern.md`
- `ADR-0035-parser-simple-call-subset-and-dot-fallback.md`
- `ADR-0036-parser-dot-call-subset-with-chain-fallback.md`
- `ADR-0037-parser-shadow-summary-artifact.md`
- `ADR-0038-parser-dot-chain-rejection-boundary.md`
- `ADR-0039-parser-function-def-entry-subset.md`
- `ADR-0040-parser-m2-shadow-gate.md`
- `ADR-0041-parser-m2-full-shadow-gate.md`
- `ADR-0042-parser-hash-simple-func-subset.md`
- `ADR-0043-parser-m2-artifactized-shadow-gate.md`
- `ADR-0044-shadow-gate-script-generalization.md`
- `ADR-0045-parser-hash-signature-subset-alignment.md`
- `ADR-0046-parser-hash-body-stmt-or-expr-subset.md`
- `ADR-0047-parser-hash-assign-expression-subset.md`
- `ADR-0048-parser-hash-arrow-no-assign-subset.md`
