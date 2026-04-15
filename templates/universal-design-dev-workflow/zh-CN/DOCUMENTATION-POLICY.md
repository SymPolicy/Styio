# 文档策略

**文档作用：** 开发类 Markdown 的存放、**单一事实来源（SSOT）**、历史/里程碑/测试与自动化对齐。不含产品语义（见 `DOMAIN-OR-PRODUCT-SPEC.md`）。**规范依据：** [`./global/SSOT-AND-MAINTENANCE-RULES.md`](./global/SSOT-AND-MAINTENANCE-RULES.md)。

**Last updated：** （分叉时填写）

## 第 0 节 — 原则

**0.1 文档作用：** 文首说明阅读理由与不涵盖范围。

**0.2 最小改动：** 优先扩展章节并加链接。

**0.3 三处去重：** 多篇重复时指定唯一 SSOT，其余改为链接。

**0.4 SSOT 速查**

| 主题 | 权威文档 | 其他文档应 |
|------|----------|------------|
| 产品/域 | `DOMAIN-OR-PRODUCT-SPEC.md` | 链接，勿长篇复述 |
| 依赖 | `THIRD-PARTY.md` | 与锁文件一致 |
| 里程碑批次 | `milestones/<YYYY-MM-DD>/00-Milestone-Index.md` | 勿私设平行总表 |
| ADR | `architecture/ADR-NNNN-*.md` | 仅链接 |
| 测试 | `tests/TEST-CATALOG.md` | 每检查一行+命令 |
| 文档拓扑 | 本文件 | 链接至此 |
| 贡献者 | `CONTRIBUTOR-AND-AGENT-SPEC.md` | 链接至此 |
| 开放问题 | `OPEN-QUESTIONS-AND-HUMAN-INPUT.md` | 关闭前移出或归档 |

## 第 1 节 — 历史

| 规则 | 说明 |
|------|------|
| 路径 | `history/YYYY-MM-DD.md` |
| 索引 | `history/README.md` |

## 第 2 节 — 里程碑

| 规则 | 说明 |
|------|------|
| 路径 | `milestones/<YYYY-MM-DD>/` |
| 文件 | `00-Milestone-Index.md`、`M*.md` |
| 验收 | 须出现在 `TEST-CATALOG.md` 或标 **gap** |

## 第 3 节 — 测试目录

按功能域；登记构建/CI 与目录双线。

## 第 4 节 — 贡献者规程

与 `CONTRIBUTOR-AND-AGENT-SPEC.md` 冲突时，**先**更新本策略。

## 第 5 节 — 可选校验

测试通过；索引含 `Last updated`；路径存在或标待定。
