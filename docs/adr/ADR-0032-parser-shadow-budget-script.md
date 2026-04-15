# ADR-0032: Parser Shadow 差异预算脚本化

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0032: Parser Shadow 差异预算脚本化.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E.6 需要“可量化的差异预算”而不只是单条测试通过。仅依赖一个 `gtest` 用例会丢失汇总信息（match/mismatch/shadow_error 数量）和可下载工件目录结构，CI 失败后定位效率不高。

## Decision

1. 新增脚本 `scripts/parser-shadow-suite-gate.sh`：
   - 对 `tests/milestones/m1/t*.styio` 执行双引擎 shadow compare（总计 2N 次）；
   - 为每次执行写入独立 artifact 目录；
   - 统计并输出：
     - `match`
     - `mismatch`
     - `shadow_error`
2. Gate 判定规则：
   - `mismatch == 0`
   - `shadow_error == 0`
   - artifact JSONL 数量不少于运行次数
3. CI 集成：
   - 新增 `Parser shadow gate (M1 full suite + artifacts)` 步骤；
   - 失败时自动上传 `build/parser-shadow-artifacts` 便于回放。

## Alternatives

1. 继续使用单条 `ctest` gate，不输出预算统计。
2. 仅在本地手工运行脚本，不纳入 CI。
3. 直接在 C++ 测试里实现统计，不保留独立脚本入口。

## Consequences

1. 差异预算从“隐式通过”变成“可量化输出”。
2. CI 失败后可直接下载工件回放，减少排查耗时。
3. 脚本与测试双轨共存，维护成本略增，但可观测性明显提升。
