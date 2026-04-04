# ADR-0045: NewParser Hash 签名子集对齐 M2 语法

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

`ADR-0042` 已让 NewParser 尝试解析 `#` simple-func 子集，但语法顺序仍偏离当前主流样式（M2 用例为 `# name [: Ret] :=/=(params) => body`），导致 typed-return / block-body 主要依赖 fallback。  
同时，返回类型只支持简单 `NAME`，无法覆盖 `tuple` 与 `[|n|]`。

## Decision

1. 对齐 hash 子集的签名顺序：
   - 支持 `# name [ : Ret ] :=/=(params) => body`；
   - 兼容 `# name(params) => body` 直达形态。
2. 扩展返回类型子集：
   - `Ret ::= NAME | [|n|] | (Ret, Ret, ...)`。
3. 对 `=> { ... }` block-body 直接走 NewParser 路径（`parse_block_with_forward`），不再默认抛错触发 fallback。
4. 保持失败回退策略不变：
   - 任一步失败仍恢复游标并回退到 legacy `parse_hash_tag`。

## Alternatives

1. 保持旧子集（继续主要依赖 fallback）。
2. 一次性重写完整 `parse_hash_tag` 全语法。
3. 只加返回类型支持，不处理语法顺序和 block-body。

## Consequences

1. M2 函数定义主路径（typed-return / block-body）可由 NewParser 原生覆盖更大比例。
2. fallback 面收窄但仍可作为风险兜底，迁移节奏保持可中断。
3. 后续可在此基础上继续拆解 hash 其它分支（如 match/cases）而不扩大一次性改动面。
