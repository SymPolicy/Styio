# 设计—研发工作流套件

**文档作用：** 与技术栈解耦的模板目录。**入口：** [`../ENTRY.md`](../ENTRY.md)。**全局规约：** [`./global/README.md`](./global/README.md)。

| 路径 | 用途 |
|------|------|
| [`global/`](./global/) | SSOT、目录镜像、维护规则 |
| [`WORKFLOW-MAP.md`](./WORKFLOW-MAP.md) | 阶段与门禁 |
| [`DOCUMENTATION-POLICY.md`](./DOCUMENTATION-POLICY.md) | 文档布局与 SSOT |
| [`CONTRIBUTOR-AND-AGENT-SPEC.md`](./CONTRIBUTOR-AND-AGENT-SPEC.md) | 人机协作与自动化 |
| [`DOMAIN-OR-PRODUCT-SPEC.md`](./DOMAIN-OR-PRODUCT-SPEC.md) | 域/产品权威规格 |
| [`OPEN-QUESTIONS-AND-HUMAN-INPUT.md`](./OPEN-QUESTIONS-AND-HUMAN-INPUT.md) | 待人工填写项 |
| [`THIRD-PARTY.md`](./THIRD-PARTY.md) | 第三方依赖 |
| [`architecture/ADR.template.md`](./architecture/ADR.template.md) | 架构决策模板 |
| [`milestones/`](./milestones/) | 里程碑 |
| [`tests/`](./tests/) | 测试目录模板 |
| [`history/`](./history/) | 开发历史模板 |
| [`prompts/`](./prompts/) | 文档生成提示词 |

**落地：** 将本树（含 `global/`）复制到目标仓 `docs/`；先填写开放问题，再按 [`../ENTRY.md`](../ENTRY.md) 主提示词推进。

英文镜像见 [`../en/`](../en/)。
