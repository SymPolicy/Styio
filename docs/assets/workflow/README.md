# 工作流资产索引

**文档作用：** 索引 `docs/assets/workflow/` 下的可复用工程流程、测试框架说明与仓库卫生标准，供后续 checkpoint 与维护工作直接引用。

**Last updated:** 2026-04-08

| 文件 | 用途 |
|------|------|
| [`CHECKPOINT-WORKFLOW.md`](./CHECKPOINT-WORKFLOW.md) | 中断友好 checkpoint 执行规则 |
| [`TEST-CATALOG.md`](./TEST-CATALOG.md) | 语言功能测试目录与 `ctest` 入口 |
| [`FIVE-LAYER-PIPELINE.md`](./FIVE-LAYER-PIPELINE.md) | 五层编译流水线 golden 规范 |
| [`REPO-HYGIENE-COMMIT-STANDARD.md`](./REPO-HYGIENE-COMMIT-STANDARD.md) | `.gitignore`、提交、push、大文件与历史重写标准 |

## 命名规则

1. 可复用 workflow 资产使用稳定、可搜索的全大写短横线命名。
2. 新增 workflow 文档前，先确认内容不是某次 checkpoint 的临时记录；若是临时记录，应写入 `docs/history/` 或 `docs/adr/`。
