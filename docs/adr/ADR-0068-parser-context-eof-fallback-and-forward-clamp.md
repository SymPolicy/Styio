# ADR-0068: ParserContext 在 token 越界时降级为 EOF，并钳制前移步长

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0068: ParserContext 在 token 越界时降级为 EOF，并钳制前移步长.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

`StyioContext` 的 token 访问路径使用了多处 `tokens.at(index_of_token)`。  
在异常边界（例如空 token 向量、或 `move_forward` 超过 token 尾部）会抛出 `std::out_of_range("vector")`，导致：

1. 错误语义不稳定（暴露 STL 内部异常文案）。
2. parser 安全回归无法覆盖“空 token / 过移”这类上下文损坏场景。

新增红灯测试已复现：

- `StyioSecurityParserContext.EmptyTokenVectorFallsBackToEofToken`
- `StyioSecurityParserContext.MoveForwardBeyondTokenTailIsClampedToEof`

## Decision

1. 在 `StyioContext` 内引入 EOF fallback token（`TOK_EOF`）。
2. `cur_tok()` / `cur_tok_type()` 在 `index_of_token >= tokens.size()` 时返回 EOF 语义，而不是抛 `out_of_range`。
3. `move_forward()` 在尾部执行钳制（clamp），不再越界访问。
4. `map_match()` 在剩余 token 数不足时直接返回 `false`。
5. `try_match()` / `try_match_panic()` 首次匹配改为边界安全检查。

## Alternatives

1. 保持 `at()` 抛异常，并在更高层统一捕获（仍会泄露内部异常类型与文案）。
2. 在调用点零散加 `index < size`（维护成本高，易漏）。
3. 强制 tokenizer 永不返回空 token（不能覆盖外部/测试构造上下文场景）。

## Consequences

1. ParserContext 边界行为稳定化：EOF 降级优先，避免 STL 越界异常泄露。
2. 安全测试可稳定覆盖“空 token / 过移”场景并持续守门。
3. 行为变更局限在越界语义，不影响正常 token 流解析路径。
