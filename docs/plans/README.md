# 计划文档索引

**文档作用：** 索引 `docs/plans/` 下的实施计划、迁移草案与早期探索；这些文件不是语言或验收层面的 SSOT。

**Last updated:** 2026-04-08

| 文件 | 状态 | 用途 |
|------|------|------|
| [`Standard-Streams-Plan.md`](./Standard-Streams-Plan.md) | Historical plan | 标准流语法设计与实现计划草案 |
| [`Resource-Topology-v2-Implementation-Plan.md`](./Resource-Topology-v2-Implementation-Plan.md) | Active/Reference plan | Topology v2 实施清单与风险矩阵 |
| [`BoundedRing-Codegen-Adjustment.md`](./BoundedRing-Codegen-Adjustment.md) | Adjustment note | `[|n|]` 环缓 codegen 调整记录 |
| [`Early-Ideas.md`](./Early-Ideas.md) | Historical notes | 语言早期碎片想法与符号实验 |

## 命名规则

1. 新计划文件必须使用描述性名称，禁止再新增 `idea.md`、`notes.md`、`misc.md` 这类泛名文件。
2. 计划文档优先使用 `<Topic>-Plan.md`、`<Topic>-Implementation-Plan.md`、`<Topic>-Adjustment.md` 这类可搜索命名。
3. 计划文件若被冻结规格或设计 SSOT 取代，必须在文首明确写明状态。
