# ADR-0042: NewParser 优先解析 Hash SimpleFunc 子集

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0042: NewParser 优先解析 Hash SimpleFunc 子集.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

上一刀已让 NewParser 接管 `#` 入口，但语义体全部复用 legacy `parse_hash_tag`。为了逐步降低 legacy 依赖，需要先抽出最常见、风险最低的函数声明形态。

## Decision

1. 在 NewParser `#` 分支中优先尝试 simple-func 子集：
   - 形态：`# name [ : Ret ] :=/=(params) => expr`
2. 支持范围：
   - 参数列表复用 `parse_params`
   - 返回类型仅支持简单 `NAME`
   - 函数体仅支持表达式（`=> expr`）
3. 不支持形态自动回退 legacy：
   - block body（`=> { ... }`）
   - 更复杂返回类型（tuple/bounded 等）
4. 回退机制：
   - 失败时恢复游标并调用 `parse_hash_tag(context)`。

## Alternatives

1. 继续全部复用 `parse_hash_tag`。
2. 一次性重写完整 hash 函数语法。
3. 仅在 token 门层放开，不做语义尝试。

## Consequences

1. NewParser 在最常见 hash simple-func 路径上开始具备原生解析能力。
2. 复杂语法仍由 legacy 托底，风险可控。
3. 后续可按参数/返回类型/块体逐段扩展而不破坏现有行为。
