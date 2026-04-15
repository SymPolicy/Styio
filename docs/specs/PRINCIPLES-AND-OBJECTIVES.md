# Styio Principles and Objectives / Styio 项目原则与目标

[EN] Purpose: Define the project-wide principles and objectives for planning, design, development, testing, and review; when tradeoff arguments or documents conflict, this file defines Styio's priority order and rewrite boundary.

[CN] 目标：定义 Styio 在规划、设计、开发、测试与审核中的项目级原则与目标；当取舍理由或文档发生冲突时，本文件定义 Styio 的优先级顺序与重写边界。

[EN] Last updated: 2026-04-15

[CN] 更新日期：2026-04-15

[EN] Status: Active project-governance SSOT

[CN] 状态：当前有效的项目治理级 SSOT

---

## [CN] 简体中文

### [CN] 如何使用

1. 只要路线图、设计、实现、测试或审核开始讨论取舍，先适用本文件。
2. 局部流程文档可以细化执行方式，但不得反转本文件规定的优先级顺序。
3. 如果代码、测试或文档与本文件冲突，必须通过修改代码、测试、文档或明确标注临时缺口来显式解决，不能默认让偶然实现上升为权威。

### [CN] 基本原则

| # | 原则 | 说明 |
|---|------|------|
| 1 | 性能第一，易用性第二 | Styio 以性能为第一优先级，以易用性为第二优先级。凡是能够提升性能或易用性的语言特性、编译器策略、运行时结构或工具链能力，都可以不计实现成本地推进。Styio 不会因为重构困难、迁移周期长或兼容性破坏而放弃有价值的特性；如果必要，整个项目可以净室重写。 |
| 2 | 设计意图高于偶然实现 | 现有行为不会仅因为“已经存在”就自动成为权威；有价值的目标设计可以要求替换或删除当前实现的大块结构。 |
| 3 | 全系统优化高于局部便利 | 当性能或易用性受到影响时，parser、analyzer、IR、codegen、runtime、IDE、工具链、文档与流程应被作为一个系统整体评估。 |
| 4 | 兼容性是工具，不是目标 | legacy 路径、影子路由和迁移桥接只在它们能够帮助项目到达更优终态时才有价值，并且必须带有明确的退出条件。 |
| 5 | 证据高于直觉 | 凡是涉及正确性、性能、安全性和行为变化的重要主张，都需要测试、基准、诊断和文档更新来支撑。 |
| 6 | 审核应服务长期项目价值 | 只要更符合 Styio 的优先级，大规模删除、替换、不兼容变更甚至重写，都可能是正确的审核结论；“改动太大”本身不是充分反对理由。 |
| 7 | 可恢复性管理风险，不限制目标 | Checkpoint、ADR、history 和分阶段 rollout 的存在，是为了让大胆变更可执行、可审计，而不是为了逼迫项目保持保守。 |

### [CN] 全流程指令

| 环节 | 指令 |
|------|------|
| 规划 | 应按性能收益、易用性收益和架构杠杆排序，而不是按迁移舒适度或短期改动量排序。 |
| 设计 | 应优先选择能带来更快执行、更清晰使用和更强长期演进能力的语义、语法和架构，即便这意味着不兼容变更。 |
| 开发 | 可以采用分阶段迁移、双轨路由或推倒重来。不要仅仅为了避免重构痛苦而保留偶然形成的复杂结构。 |
| 测试 | 既要证明行为正确，也要在相关场景下证明性能与安全目标。回归套件应保护战略目标，而不只是保护既有怪异行为。 |
| 审核 | 应判断变更是否让 Styio 更接近其目标。如果变更能够实质改善 Styio，那么“破坏性太大”“改动太多”“会破坏兼容性”都不足以单独构成否决理由。 |
| 文档 | 必须显式承认这套优先级。局部文档只要讨论取舍，就应引用本文件，而不是重新定义项目级优先级。 |

### [CN] 项目目标

| # | 目标 | 说明 |
|---|------|------|
| 1 | 对齐最高可实现性能 | 让 Styio 始终朝着其语言模型在现实条件下可达到的最高性能实现对齐。 |
| 2 | 让推荐写法同时是高性能写法 | 避免用户必须依赖底层绕路技巧才能获得好性能。 |
| 3 | 在不改变优先级排序的前提下提升易用性 | 通过改进语法、诊断、工具链、IDE 支持和文档质量来持续提升易用性。 |
| 4 | 让设计、实现、测试、审核和文档收敛成一个系统 | 避免漂移成多套平行真相。 |
| 5 | 保留执行大规模架构变更的能力 | 在不丢失可审计性和恢复纪律的前提下，允许破坏兼容性的迁移和净室重写。 |

### [CN] 与其他文档的关系

1. 语言语义仍以 `../design/` 为准；本文件不替代语义 SSOT。
2. 具体流程仍以 `../assets/workflow/` 为准；本文件只定义优先级顺序与项目目标。
3. 计划、里程碑、测试与审核在解释某个取舍为何成立时，都应把本文件作为共同基线。

---

## [EN] English

### [EN] How To Use

1. Apply this file first whenever roadmaps, designs, implementations, tests, or reviews debate tradeoffs.
2. Local workflow documents may refine execution details, but they must not reverse the priority order defined here.
3. If code, tests, or documents conflict with this file, resolve the conflict explicitly by updating code, tests, docs, or marking a temporary gap.

### [EN] Core Principles

| # | Principle | Description |
|---|-----------|-------------|
| 1 | Performance first, usability second | Performance is Styio's first priority; usability is second. Any language feature, compiler strategy, runtime structure, or tooling capability that improves performance or usability may be implemented regardless of implementation cost. Styio must not reject a valuable feature because the refactor is hard, the migration is long, or compatibility breaks. If necessary, the project may be rewritten from a clean room. |
| 2 | Design intent outranks accidental implementation | Existing behavior is not authoritative merely because it exists; valuable target design may require replacing or deleting large parts of the current implementation. |
| 3 | Whole-system optimization outranks local convenience | Parser, analyzer, IR, codegen, runtime, IDE, tooling, docs, and workflow should be judged as one system when performance or usability is at stake. |
| 4 | Compatibility is a tool, not a goal | Legacy paths, shadow routes, and migration bridges are acceptable only when they help the project reach a better end state, and they should carry explicit exit criteria. |
| 5 | Proof outranks intuition | Important claims about correctness, performance, safety, and behavior need tests, benchmarks, diagnostics, and documentation updates. |
| 6 | Review should serve long-term project value | A correct review may approve large-scale deletion, replacement, incompatibility, or rewrite when those moves better serve Styio's priorities; "too disruptive" alone is not a sufficient objection. |
| 7 | Recoverability manages risk, not ambition | Checkpoints, ADRs, history logs, and phased rollout exist to make bold changes executable and auditable, not to force the project to stay conservative. |

### [EN] Lifecycle Directives

| Area | Directive |
|------|-----------|
| Planning | Rank roadmap items by expected performance gain, usability gain, and architectural leverage, not by migration comfort or short-term code churn. |
| Design | Prefer semantics, syntax, and architecture that enable faster code, clearer usage, and stronger long-term evolution, even when they force incompatible changes. |
| Development | Implementation may use phased migration, dual-track routing, or clean-slate replacement. Do not preserve accidental complexity merely to avoid refactoring pain. |
| Testing | Tests must prove both behavioral correctness and, where relevant, the intended performance and safety properties. Regression suites should protect strategic goals, not only existing quirks. |
| Review | Reviews should ask whether a change moves Styio closer to its objectives. "Too disruptive", "too large", or "breaks compatibility" is not a sufficient objection if the change materially improves Styio. |
| Documentation | Documents must make the priority order explicit. When a local document discusses tradeoffs, it should reference this file instead of redefining project-wide priorities. |

### [EN] Project Objectives

| # | Objective | Description |
|---|-----------|-------------|
| 1 | Align Styio with the highest practical performance ceiling | Keep Styio aligned with the highest-performance implementation that is practically achievable for its language model. |
| 2 | Make the preferred way also be the fast way | Users should not need low-level workaround patterns to obtain good performance. |
| 3 | Increase ease of use without changing the priority order | Improve syntax, diagnostics, tooling, IDE support, and documentation quality without sacrificing the performance-first ordering. |
| 4 | Keep the project converging toward one coherent system | Design, implementation, tests, reviews, and documentation should not drift into parallel truths. |
| 5 | Preserve the ability to execute large architectural change | Compatibility-breaking migration and clean-room rewrite remain valid options as long as auditability and recovery discipline are preserved. |

### [EN] Relationship To Other Documents

1. Language semantics remain in `../design/`; this file does not replace semantic SSOT.
2. Workflow details remain in `../assets/workflow/`; this file only defines priority order and objectives.
3. Plans, milestones, tests, and reviews must use this file as the common baseline when explaining why a tradeoff is acceptable.

---
