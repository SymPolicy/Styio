# SSOT 与维护规则（规范性）

**文档作用：** 文献卫生与单一事实来源约定；本树中的 `DOCUMENTATION-POLICY.md` 须与之对齐，除非项目书面另有规定。

## 「文档作用」行

文首尽早说明：**读者为何阅读本文**、**本文不涵盖什么**。

## 最小改动

能加链扩写则不另起平行长文。

## 「三处」去重规则

若**三篇及以上**文档对同一事实作**实质性**重复：

1. 指定**唯一 SSOT**。  
2. 无合适承载处再增新文。  
3. 其余处删改重复内容，改为链接。

## SSOT 槽位（分叉时自定义）

| 主题 | 权威文档 | 其他文档应 |
|------|----------|------------|
| 产品/域行为 | `DOMAIN-OR-PRODUCT-SPEC.md` | 链接、短摘要 |
| 依赖 | `THIRD-PARTY.md` | 与锁文件一致 |
| 里程碑批次 | `milestones/<YYYY-MM-DD>/00-Milestone-Index.md` | 勿随意复制总表 |
| 架构决策 | `architecture/ADR-NNNN-*.md` | 仅链接 |
| 测试 | `tests/TEST-CATALOG.md` | 每检查一行+命令 |
| 文档拓扑 | `DOCUMENTATION-POLICY.md` | 链接至此 |
| 贡献者规程 | `CONTRIBUTOR-AND-AGENT-SPEC.md` | 链接至此 |
| 开放问题 | `OPEN-QUESTIONS-AND-HUMAN-INPUT.md` | 关闭前保持链接 |

## 历史目录

- 路径：`history/YYYY-MM-DD.md`  
- 索引：`history/README.md`

## 里程碑目录

- 路径：`milestones/<YYYY-MM-DD>/`  
- 文件：`00-Milestone-Index.md`、分段 `M*.md`  
- 验收：`TEST-CATALOG.md` 有名或标 **gap**

## 测试目录

- 按功能域分组。  
- 每行：测试 id、输入、判据、自动化命令。  
- 在**一处**构建/CI 清单与本目录**双登记**。

## 策略与贡献者规程冲突

若 `DOCUMENTATION-POLICY.md` 与 `CONTRIBUTOR-AND-AGENT-SPEC.md` 不一致，**先改策略与索引**。

## 可选校验

- 文档化测试通过。  
- 历史/里程碑索引含 `Last updated`。  
- `TEST-CATALOG.md` 中路径存在或标为待定。
