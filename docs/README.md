# Styio 文档目录

**文档作用：** 作为 `docs/` 的总索引，定义文档分类边界与入口；具体语义、流程和验收规则仍以下层 SSOT 文档为准。

**Last updated:** 2026-04-08

## 目录约定

| 路径 | 内容边界 | 入口文档 |
|------|----------|----------|
| `docs/design/` | 语言设计、语法、符号、资源与标准库等设计级 SSOT | [`design/README.md`](./design/README.md) |
| `docs/specs/` | 贡献者/智能体规范、文档策略、第三方依赖约束 | [`specs/README.md`](./specs/README.md) |
| `docs/review/` | review 发现、设计冲突、待决议问题 | [`review/README.md`](./review/README.md) |
| `docs/plans/` | 设计草案、实施计划、迁移计划；不是语言或验收 SSOT | [`plans/README.md`](./plans/README.md) |
| `docs/assets/workflow/` | 可复用工作流、测试框架说明、checkpoint/提交流程 | [`assets/workflow/README.md`](./assets/workflow/README.md) |
| `docs/assets/templates/` | 可复用模板，供新 checkpoint / 重构切片直接套用 | [`assets/templates/README.md`](./assets/templates/README.md) |
| `docs/adr/` | 架构决策记录 | [`README.md`](./adr/README.md) |
| `docs/history/` | 按日期记录的开发历史与恢复指引 | [`README.md`](./history/README.md) |
| `docs/milestones/` | 按冻结批次归档的里程碑规格与验收说明 | [`README.md`](./milestones/README.md) |

## 推荐阅读顺序

1. 文档分类与维护规则：[`specs/DOCUMENTATION-POLICY.md`](./specs/DOCUMENTATION-POLICY.md)
2. 贡献者与智能体执行规范：[`specs/AGENT-SPEC.md`](./specs/AGENT-SPEC.md)
3. 资产目录与复用入口：[`assets/README.md`](./assets/README.md)
4. 中断友好工作流：[`assets/workflow/CHECKPOINT-WORKFLOW.md`](./assets/workflow/CHECKPOINT-WORKFLOW.md)
5. 测试与流水线索引：[`assets/workflow/TEST-CATALOG.md`](./assets/workflow/TEST-CATALOG.md)、[`assets/workflow/FIVE-LAYER-PIPELINE.md`](./assets/workflow/FIVE-LAYER-PIPELINE.md)
6. 语言与语法 SSOT：[`design/Styio-Language-Design.md`](./design/Styio-Language-Design.md)、[`design/Styio-EBNF.md`](./design/Styio-EBNF.md)

## 维护规则

1. 语言设计相关文档只放 `docs/design/`。
2. 可复用流程和模板只放 `docs/assets/`。
3. review 发现与冲突清单只放 `docs/review/`。
4. agent / contributor / 文档规范只放 `docs/specs/`。
5. 计划文档与冻结规格分离：计划进 `docs/plans/`，冻结批次进 `docs/milestones/<YYYY-MM-DD>/`。
