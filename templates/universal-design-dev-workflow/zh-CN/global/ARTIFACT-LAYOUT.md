# 制品布局与语言树镜像

**权威性：** 约定本套件的目录组织方式。`en/` 与 `zh-CN/` **相对路径与文件名一致**（镜像）。每个语言树内各有独立 **`global/`**。

## 套件根目录

```
templates/universal-design-dev-workflow/
├── ENTRY.md
├── README.md
├── en/
│   └── global/
└── zh-CN/
    └── global/
```

## 本语言树内（`zh-CN/`）

```
zh-CN/
├── global/
├── README.md
├── WORKFLOW-MAP.md
├── DOCUMENTATION-POLICY.md
├── CONTRIBUTOR-AND-AGENT-SPEC.md
├── DOMAIN-OR-PRODUCT-SPEC.md
├── OPEN-QUESTIONS-AND-HUMAN-INPUT.md
├── THIRD-PARTY.md
├── architecture/
├── milestones/
├── tests/
├── history/
└── prompts/
```

## 稳定制品 ID

| ID | 典型文件 |
|----|----------|
| `WORKFLOW_MAP` | `WORKFLOW-MAP.md` |
| `DOC_POLICY` | `DOCUMENTATION-POLICY.md` |
| `CONTRIBUTOR_SPEC` | `CONTRIBUTOR-AND-AGENT-SPEC.md` |
| `DOMAIN_SPEC` | `DOMAIN-OR-PRODUCT-SPEC.md` |
| `OPEN_QUESTIONS` | `OPEN-QUESTIONS-AND-HUMAN-INPUT.md` |
| `THIRD_PARTY` | `THIRD-PARTY.md` |
| `MILESTONE_BATCH` | `milestones/<YYYY-MM-DD>/00-Milestone-Index.md` |
| `MILESTONE_SEGMENT` | `milestones/<YYYY-MM-DD>/M*.md` |
| `TEST_CATALOG` | `tests/TEST-CATALOG.md` |
| `HISTORY_DAY` | `history/YYYY-MM-DD.md` |
| `ADR` | `architecture/ADR-NNNN-*.md` |

## 落地模式

| 模式 | 说明 |
|------|------|
| A | 将 `zh-CN/`（含 `global/`）复制到产品仓 `docs/`。 |
| B | 在产品仓并行维护 `docs/zh-CN/` 与 `docs/en/`。 |
| C | 以子模块等形式保留套件；从根目录 `ENTRY.md` 进入。 |

模板内链假设**本语言根目录**即文档根。
