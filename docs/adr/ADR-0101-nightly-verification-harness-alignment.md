# ADR-0101: Verification Harness 对齐 Nightly Parser

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0101: Verification Harness 对齐 Nightly Parser.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-07

## Context

虽然 CLI 默认 parser 已经切到 `nightly`，并且 `m1/m2/m5/m7` 的 shadow gate 已经进入恢复脚本，但仍有两个关键验证入口直接调用 `legacy` parser：

1. `src/StyioTesting/PipelineCheck.cpp` 的五层流水线 L2 parser
2. `tests/fuzz/fuzz_parser.cpp` 的 parser fuzz target

这会带来两个问题：

1. `styio_pipeline` 通过并不等于 nightly parser 的 AST/IR 路径真的通过；
2. `fuzz_parser_smoke` 只打 legacy，会让 nightly parser 在随机输入上的健壮性形成盲区。

## Decision

1. `PipelineCheck.cpp` 的 parser 层改为：
   - `parse_main_block_with_engine_latest(..., StyioParserEngine::Nightly, ...)`
2. `fuzz_parser.cpp` 不再只 fuzz legacy，而是同一输入顺序驱动：
   - `StyioParserEngine::Legacy`
   - `StyioParserEngine::Nightly`
3. 维持现有 fuzz target 名称不变：
   - `styio_fuzz_parser`
   - `fuzz_parser_smoke`
4. 文档同步明确：
   - 五层流水线的 L2 parser 以 nightly 为准
   - parser fuzz 现在覆盖 legacy/nightly 两条路由

## Alternatives

1. 继续保留 legacy-only harness：
   - 拒绝。这与当前 nightly-default 的主行为不一致。
2. 新增一个独立 `styio_fuzz_parser_nightly`，保留旧 target 不动：
   - 暂不采用。当前阶段一个 target 同时覆盖两条引擎，能以更低维护成本消除盲区。
3. 直接删除 legacy parser 在 fuzz 中的覆盖：
   - 拒绝。legacy 仍是 shadow/回退路径，需要继续被随机输入触达，直到真正退场。

## Consequences

1. `styio_pipeline` 现在对 nightly parser 的 AST 层有直接约束，而不是间接依赖 CLI 默认行为。
2. `fuzz_parser_smoke` 对 parser 健壮性的覆盖从单引擎提升到双引擎。
3. 后续若要移除 `legacy` 主路径，验证入口的剩余 legacy 依赖会更少，观察窗口也更接近真实生产路径。
