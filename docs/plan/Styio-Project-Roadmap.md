# Styio Project Roadmap

**Purpose:** Present one reviewable project roadmap that separates established foundations, current compiler-correctness work, proposed optimization checkpoints, and release coordination.

**Last updated:** 2026-08-01

**Plan status:** Approved baseline. P0 and the first P1 compiler-correctness closure are complete; proposed or deferred checkpoints still require explicit selection before implementation.

## 前置条件

1. 已批准的实现必须按最小可独立验收闭环执行；未选择的候选检查点保持 `deferred`。
2. 每个 Better Plan group 只运行一次全链路 Reviewer；完整回归只在评审后运行，失败后仅做故障驱动重试。
3. 子智能体只在已批准的 Better Plan group 内按 Designer、Worker、Verifier、Reviewer 角色边界启动，不得自行扩大到延后检查点。
4. 当前能力、已完成状态和技术限制只能来自 `CURRENT-STATE.md`、当前 gap ledger、设计 SSOT、测试目录与代码证据。
5. 公共基座、工作流或测试框架发生实际变更时，先依据 Delivery Governance 能力事实创建独立 foundation closure；纯产品工作不得顺手扩张公共基座。
6. 并行: 评审阶段只允许互不修改状态的证据核对并行；语义决策、计划状态写入、下一实现闭环选择和最终 SSOT 更新保持串行。

## 1. 结构原则

1. 一个总路线图只负责范围、阶段和评审入口，不复制各技术 SSOT。
2. 永久性治理规则是已建立的 foundation，不应长期伪装成 `in_progress` 任务。
3. 编译器正确性优先于性能改写：P0 placeholder inventory 与首个 IR loop-control legality 闭环已经完成，后续仍一次只选择一个独立实现簇。
4. 算法优化按真实依赖拆分。P0 完成后，Parser C、IR Pass D、Type Solver E 可以作为三个独立候选前沿；Resource F 依赖 E，Stream G 依赖 F。
5. 生态发布不默认等待全部长期优化工作。维护者先选择 release scope，再冻结修订并运行一次最终回归与一次跨仓库矩阵。
6. 每个实现阶段在获批后再细化为 Better Plan `group`：一个设计节点、一个或多个互不冲突的实现节点、一个最终验证节点。

## 2. 计划树

```text
Delivery Governance                          [已建立能力事实；不是 Plan]

Styio Project Roadmap                         [已批准基线]
├── Compiler Correctness                     [P0 + 首个 P1 闭环已完成]
│   └── IR Loop-Control Legality              [已验收]
├── Algorithmic Optimization                 [A/B 已完成；后续等待评审]
│   ├── C Parser Core                        [依赖 P0 + B]
│   ├── D Verified IR Passes                 [依赖 P0 + A]
│   ├── E Type Constraint Engine             [依赖 P0]
│   ├── F Resource Typestate/Dataflow        [依赖 E]
│   └── G Stream Concurrency                 [依赖 F]
└── Ecosystem Release                        [等待 release scope]
    ├── Freeze Candidate Revisions
    └── Final Regressions + Cross-repo Matrix
```

## 3. 当前事实与拟议工作

| 范围 | 当前事实 | 下一步提案 | 状态 |
|------|----------|------------|------|
| Delivery Governance | 文档、工作流、测试和交付门禁已有明确入口；它是能力事实，不是 Plan | 仅在具体交付发现共享基座缺口时创建独立 foundation closure | 已建立 |
| Compiler Correctness | P0 已完成；IR verifier 现已区分最终根与显式中间 fragment，合法循环控制保持可用，最终根顶层 break/continue fail-closed | 从剩余显式 feature debt 或 C/D/E 候选前沿中再选择一个独立闭环 | 首个 P1 已完成 |
| Optimization A/B | IR Walker/Pass Manager 与 span-first tokenizer 已有当前状态证据 | 不做追溯性重写，仅作为后续节点的已完成前置事实 | 已完成 |
| Optimization C/D/E | Parser、IR passes、type solver 是不同模块边界 | P0 后允许独立评审和并行准备，任何代码执行仍需单独批准 | 待评审 |
| Optimization F/G | Resource dataflow 与 stream concurrency 涉及状态、所有权和并发安全 | F 在 E 后，G 在 F 后；每次只关闭一个场景 | 待评审 |
| Ecosystem Release | 产品归属与机器契约已冻结，固定修订验收尚未完成 | 维护者先决定本次 release 是否包含 C–G 中的任何节点 | 延后 |

## 4. 阶段闭环

### Capability Baseline — Delivery Governance

该能力事实代表稳定的路由规则与最小门禁，不进入 Manifest，也不持续占用执行队列。共享脚本、公共契约或测试框架必须作为单独闭环处理，并在同一变更中更新 owner、生成投影和回归入口。

### Stage 1 — Compiler Correctness

P0 清单已将 placeholder 分类为 dead syntax、intentional no-op、typed unsupported boundary 或 implementation debt。首个 P1 选择并完成了 IR loop-control legality：最终 IR 根严格拒绝循环外控制，中间构造 fragment 仅在显式上下文中延后该判断，既有合法循环与清理语义保持不变。后续继续一次只选择一个簇。

### Stage 2 — Algorithmic Optimization

Checkpoint C、D、E 不建立人为串行关系：它们分别归属 parser、verified IR passes、type solving。每个检查点必须先对比成熟开源实现，说明算法、数据结构、缓存/失效策略、复杂度和内存边界，再用 Styio 自身正确性与 benchmark 证据决定是否采用。F 和 G 保持串行，因为 stream concurrency 必须建立在明确的 typestate/dataflow 边界之上。

### Stage 3 — Ecosystem Release

发布阶段先决定 release scope，而不是默认把所有 backlog 都塞进一次发布。范围确认后冻结不可变修订，每个受影响仓库只跑一次最终全量回归，然后运行一次跨仓库验收矩阵。失败只回到对应 owner 的最小修复闭环。

## 5. 并行与串行边界

| 工作 | 可并行 | 必须串行 |
|------|--------|----------|
| 后续 correctness inventory | 不同 feature-debt 家族的证据核对可分离 | 下一实现簇选择、语义冻结与最终验收 |
| C / D / E 准备 | 开源实现调研、基准输入、现状证据、测试发现 | 每个检查点自己的公共语义或接口决策 |
| F / G | F 的文档与测试发现可提前进行 | E → F → G 的语义与实现交付 |
| Ecosystem release | 各 owner 的 focused evidence 可并行收集 | release scope → revision freeze → final regressions → matrix |

## 6. 请维护者反馈的决策

1. 下一闭环应选择剩余 compiler-correctness feature debt、Parser C、Verified IR Pass D，还是 Type Solver E？C、D、E 仍按独立候选前沿处理。
2. 下一次 ecosystem release 应只基于当前功能基线，还是必须包含 C–G 中的某个明确检查点？
3. F → G 的串行关系是否符合预期，还是 stream 的某个纯运行时子场景可以脱离 resource typestate 单独推进？

## 验收条件

1. 维护者已确认阶段边界并批准首个实现闭环；后续候选保持显式延后，直到再次选择。
2. `Capabilities.json`、`Manifest.json` 和各 `Checkpoints.json` 只表达本路线图中的结构，不保留旧计划的兼容节点或重复权威。
3. Better Plan 默认树与详情树能一致展示 ownership、状态、真实依赖和显式延后项。
4. `python3 scripts/manifest_tool.py validate docs/plan --check-sources`、文档审计、生命周期、团队文档、仓库卫生与隐私门禁保持通过。
