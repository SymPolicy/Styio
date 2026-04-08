# 资产目录索引

**文档作用：** 索引 `docs/assets/` 下可复用的工作流与模板资产，约束它们的职责边界；不记录具体里程碑历史与语言语义。

**Last updated:** 2026-04-08

## 子目录

| 路径 | 内容 | 入口 |
|------|------|------|
| `workflow/` | 可直接执行或复用的工程流程、测试框架说明、仓库卫生标准 | [`workflow/README.md`](./workflow/README.md) |
| `templates/` | 可复制的模板骨架，用于新 checkpoint、重构切片或通用流程落地 | [`templates/README.md`](./templates/README.md) |

## 使用规则

1. 能被多个里程碑复用的流程文档，放 `docs/assets/workflow/`。
2. 能被多个重构或新项目直接套用的模板，放 `docs/assets/templates/`。
3. `docs/assets/` 不承载项目历史，不写某一天的实施记录；这类内容仍写入 `docs/history/`。
