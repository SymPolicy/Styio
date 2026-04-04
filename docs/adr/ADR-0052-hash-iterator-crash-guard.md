# ADR-0052: Hash Iterator Definition 临时 Crash Guard

- **Status:** Accepted
- **Date:** 2026-04-05
- **Superseded by:** `ADR-0053-param-ctor-null-type-and-hash-iterator-restore.md`

## Context

`# f(x) >> ...` 在 CLI 路径上会触发 legacy/new 同步段错误（exit 139）。  
该问题不应继续暴露为进程崩溃，需先收口为稳定可诊断错误，再拆分 runtime 修复。

## Decision

1. 在 legacy `parse_hash_tag` 与 NewParser `parse_hash_stmt_new_subset` 的 `ITERATOR` 分支统一抛出：
   - `StyioNotImplemented("hash iterator definitions are temporarily disabled (runtime crash guard)")`。
2. 保持错误分类稳定：
   - 由 CLI 统一映射为 `ParseError`（退出码 3）。
3. 新增回归：
   - security：`RejectsHashIteratorFunctionDefWithStableError`。
   - parser engine：`HashIteratorDefinitionReturnsParseErrorWithoutCrash`（双引擎 JSONL 断言）。

## Alternatives

1. 保持现状，允许段错误继续出现。
2. 直接尝试一次性修复 iterator runtime 崩溃。
3. 仅在 NewParser 分支加 guard，legacy 保持原行为。

## Consequences

1. 用户侧行为从“进程崩溃”收敛为“稳定 ParseError”。
2. `# ... >> ...` 定义语法暂时禁用，直到 runtime 崩溃根因修复。
3. 后续可单独开 D 阶段稳定性切片处理 runtime 实现，不阻塞 parser 子集继续演进。
