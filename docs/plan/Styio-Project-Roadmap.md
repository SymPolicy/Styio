# Styio Project Roadmap

**Purpose:** Present one reviewable project roadmap that separates established foundations, current compiler-correctness work, proposed optimization checkpoints, and release coordination.

**Last updated:** 2026-08-03

**Plan status:** Approved baseline. Compiler-correctness and optimization delivery groups in the Manifest are complete. Ecosystem Release is closed as Styio-only functional acceptance; cross-repository matrix work is waived for this candidate.

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
5. 本次发布边界为 Styio-only：维护者已冻结本仓功能验收为唯一门禁，并明确排除跨仓库生态互通矩阵与 Pafio/Platform/Vityo 参与。
6. 每个实现阶段在获批后再细化为 Better Plan `group`：一个设计节点、一个或多个互不冲突的实现节点、一个最终验证节点。

## 2. 计划树

```text
Delivery Governance                          [已建立能力事实；不是 Plan]

Styio Project Roadmap                         [已批准基线]
├── Compiler Correctness                     [P0 + 首个 P1 闭环已完成]
│   └── IR Loop-Control Legality              [已验收]
├── Algorithmic Optimization                 [A–G 交付组已完成]
│   ├── C Parser Core                        [已验收]
│   ├── D Verified IR Passes                 [已验收]
│   ├── E Type Constraint Engine             [已验收]
│   ├── F Resource Typestate/Dataflow        [已验收]
│   └── G Stream Concurrency                 [已验收]
└── Ecosystem Release                        [Styio-only 已关闭]
    ├── RELEASE-SCOPE Styio-only functional acceptance [已完成]
    └── RELEASE-MATRIX Cross-repo matrix [已跳过]
```

## 3. 当前事实与拟议工作

| 范围 | 当前事实 | 下一步提案 | 状态 |
|------|----------|------------|------|
| Delivery Governance | 文档、工作流、测试和交付门禁已有明确入口；它是能力事实，不是 Plan | 仅在具体交付发现共享基座缺口时创建独立 foundation closure | 已建立 |
| Compiler Correctness | P0 已完成；IR verifier 现已区分最终根与显式中间 fragment，合法循环控制保持可用，最终根顶层 break/continue fail-closed | 从剩余显式 feature debt 中再选择一个独立闭环 | 首个 P1 已完成 |
| Optimization A/B | IR Walker/Pass Manager 与 span-first tokenizer 已有当前状态证据 | 不做追溯性重写，仅作为后续节点的已完成前置事实 | 已完成 |
| Optimization C/D/E | Parser、IR passes、type solver 交付组已在 Manifest 中完成 | 仅在新证据要求时开独立修复或扩展闭环 | 已完成 |
| Optimization F/G | Resource dataflow 与 stream concurrency 交付组已在 Manifest 中完成 | 仅在新场景或预存功能债需要时再开独立闭环 | 已完成 |
| Ecosystem Release | 产品归属已冻结；维护者选择 Styio-only，跨仓矩阵已跳过 | 以 Styio 本仓全量功能回归为唯一发布验收；pipeline 预存失败另开修复 | 已完成 |

## 4. 阶段闭环

### Capability Baseline — Delivery Governance

该能力事实代表稳定的路由规则与最小门禁，不进入 Manifest，也不持续占用执行队列。共享脚本、公共契约或测试框架必须作为单独闭环处理，并在同一变更中更新 owner、生成投影和回归入口。

### Stage 1 — Compiler Correctness

P0 清单已将 placeholder 分类为 dead syntax、intentional no-op、typed unsupported boundary 或 implementation debt。首个 P1 选择并完成了 IR loop-control legality：最终 IR 根严格拒绝循环外控制，中间构造 fragment 仅在显式上下文中延后该判断，既有合法循环与清理语义保持不变。后续继续一次只选择一个簇。

### Stage 2 — Algorithmic Optimization

Checkpoint C、D、E 不建立人为串行关系：它们分别归属 parser、verified IR passes、type solving。每个检查点必须先对比成熟开源实现，说明算法、数据结构、缓存/失效策略、复杂度和内存边界，再用 Styio 自身正确性与 benchmark 证据决定是否采用。F 和 G 保持串行，因为 stream concurrency 必须建立在明确的 typestate/dataflow 边界之上。

### Stage 3 — Ecosystem Release

维护者已决定本次候选为 Styio-only：不要求生态互通验收。参与仓库仅为 `styio-nightly`；验收是一次本仓全量功能回归。跨仓库矩阵节点已跳过，失败只回到 Styio owner 的最小修复闭环。

## 5. 并行与串行边界

| 工作 | 可并行 | 必须串行 |
|------|--------|----------|
| 后续 correctness inventory | 不同 feature-debt 家族的证据核对可分离 | 下一实现簇选择、语义冻结与最终验收 |
| C / D / E 准备 | 开源实现调研、基准输入、现状证据、测试发现 | 每个检查点自己的公共语义或接口决策 |
| F / G | F 的文档与测试发现可提前进行 | E → F → G 的语义与实现交付 |
| Ecosystem release | 不适用：本次无 Styio 本仓验收 | Styio-only scope 已冻结；跨仓矩阵已跳过 |

## 6. 已关闭的维护者决策

1. 本次发布不做生态互通验收；Styio 保证本仓功能验收即可。
2. 参与仓库：仅 `styio-nightly`。显式排除 Pafio、Platform、Vityo 矩阵参与。
3. 候选冻结条件：以受测 Styio 修订为候选；唯一验收是一次 Styio 全量功能回归。

## 验收条件

1. 维护者已确认阶段边界并批准首个实现闭环；后续候选保持显式延后，直到再次选择。
2. `Capabilities.json`、`Manifest.json` 和各 `Checkpoints.json` 只表达本路线图中的结构，不保留旧计划的兼容节点或重复权威。
3. Better Plan 默认树与详情树能一致展示 ownership、状态、真实依赖和显式延后项。
4. `python3 scripts/manifest_tool.py validate docs/plan --check-sources`、文档审计、生命周期、团队文档、仓库卫生与隐私门禁保持通过。
