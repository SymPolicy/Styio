# ADR-0038: Dot-Chain 保持一致拒绝边界

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

NewParser 已接管 dot-call 子集（`name.method(args...)`），但链式调用 `foo.bar(1).baz(2)` 在 legacy 仍不支持。若边界未锁定，后续改动可能导致 new/legacy 在失败语义上漂移。

## Decision

1. 新增 `StyioParserEngine.DotChainStillRejectedConsistentlyAcrossEngines` 回归。
2. 对同一输入分别执行：
   - `--parser-engine=legacy`
   - `--parser-engine=new`
3. 断言两者都返回 `ParseError`（退出码 `3`，JSONL category 为 `ParseError`）。

## Alternatives

1. 只测试 new 引擎失败，不校验 legacy 一致性。
2. 忽略该边界，等待 chain-call 真正实现后再补。
3. 让 dot-chain 在 new 先行支持，legacy 暂时不一致。

## Consequences

1. Dot-chain 失败语义被显式冻结，降低回归风险。
2. 后续若要支持 chain-call，需显式更新该测试和 ADR。
