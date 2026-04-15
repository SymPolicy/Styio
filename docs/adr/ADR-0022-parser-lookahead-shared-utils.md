# ADR-0022: parser 前瞻工具共享模块

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0022: parser 前瞻工具共享模块.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E.2 目标是把 legacy/new parser 都会用到的“trivia 跳过 + 前瞻判断”能力抽离成共享工具，避免后续双轨演进时重复维护同一套 token 游标逻辑。
原实现分散在 `StyioContext::skip()` 与 `try_check()` 内，扩展成本高且不利于 new parser 直接复用。

## Decision

1. 新增模块：
   - `src/StyioParser/ParserLookahead.hpp`
   - `src/StyioParser/ParserLookahead.cpp`
2. 统一提供三类基础能力：
   - `styio_is_trivia_token(...)`
   - `styio_skip_trivia_tokens(...)`
   - `styio_try_check_non_trivia(...)`
3. legacy 路径先行接入：`StyioContext::skip()` 与 `try_check()` 改为调用共享工具。
4. 增加安全回归测试 `StyioSecurityParserLookahead.SkipTriviaFindsNextToken`。

## Alternatives

1. 继续把逻辑保留在 `StyioContext` 内部，不做抽离。
2. 直接在 new parser 中复制一份 lookahead 代码。
3. 等 new parser 完整实现后再统一重构。

## Consequences

1. new parser 可直接复用共享前瞻能力，降低双轨分叉风险。
2. legacy 行为保持不变，但实现更可维护。
3. 后续 E.3+ 在表达式子集迁移时可聚焦语法逻辑而非 token 基础设施。
