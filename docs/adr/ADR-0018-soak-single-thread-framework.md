# ADR-0018: D.1 单线程 Soak 测试框架

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0018: D.1 单线程 Soak 测试框架.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

D 阶段要求补齐“高频状态更新 + 长生命周期摄入”的稳定性基线。现有测试覆盖了：

1. milestone 功能正确性
2. security 边界与崩溃回归
3. fuzz 随机输入健壮性

但缺少可扩展的单线程 soak 框架（可在 PR 做短跑、在本地/夜间做放大量长跑）。

## Decision

1. 新增 `styio_soak_test`（GoogleTest 目标），标签 `soak;soak_smoke;single_thread`。
2. 首批用例覆盖：
   - 词法长输入摄入循环
   - 文件句柄 open/read/rewind/close 长生命周期循环
   - M6 流式程序重复执行循环
3. 用环境变量控制迭代规模（默认 smoke，支持放大量）。
4. CI 主流程新增 `ctest -L soak_smoke`。

## Alternatives

1. 只做脚本层循环，不进入 CTest。
2. 直接上高时长 deep soak 到 PR（成本高、反馈慢）。

## Consequences

1. D.1 有了可持续维护的自动化入口，且不拉长 PR 反馈过多。
2. D.2-D.4（CI 短跑 / nightly 深跑 / 失败样本沉淀）可在此框架上增量推进。
3. soak 与 security/fuzz/milestone 形成互补测试金字塔。
4. C ABI 字符串所有权契约被显式化（借用缓冲 vs 堆分配），降低误用崩溃风险。
