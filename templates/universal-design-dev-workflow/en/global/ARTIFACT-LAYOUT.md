# Artifact layout and locale parity / 制品布局与语言树镜像

### English

**Authority:** Defines kit organization. Copies under `en/` and `zh-CN/` must keep **the same relative paths and filenames** (mirror). **Global norms** live at **`en/global/`** and **`zh-CN/global/`** (duplicate content).

#### Kit root / 套件根目录

```
templates/universal-design-dev-workflow/
├── ENTRY.md          # Bilingual entry + bootstrap prompts / 双语入口与启动提示词
├── README.md         # Pointer to ENTRY / 指向 ENTRY
├── en/               # Locale tree A (mirror) / 语言树 A（镜像）
│   └── global/       # Global conventions copy 1 / 全局规约副本一
└── zh-CN/            # Locale tree B (mirror) / 语言树 B（镜像）
    └── global/       # Global conventions copy 2 / 全局规约副本二
```

#### Within each locale folder / 每个语言根目录内

```
<locale>/
├── global/
├── README.md
├── WORKFLOW-MAP.md
├── DOCUMENTATION-POLICY.md
├── CONTRIBUTOR-AND-AGENT-SPEC.md
├── DOMAIN-OR-PRODUCT-SPEC.md
├── OPEN-QUESTIONS-AND-HUMAN-INPUT.md
├── THIRD-PARTY.md
├── architecture/
│   └── ADR.template.md
├── milestones/
│   ├── README.md
│   └── _template/
├── tests/
│   └── TEST-CATALOG.template.md
├── history/
│   └── *.template.md
└── prompts/
```

#### Stable artifact IDs / 稳定制品 ID（概念层）

| ID | Typical file / 典型文件 |
|----|-------------------------|
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

#### Adoption patterns / 落地模式

**English:** (A) Copy **either** `en/*` **or** `zh-CN/*` into `docs/`; include that tree’s `global/`. (B) Bilingual product docs: mirror both trees under `docs/en/` and `docs/zh-CN/`. (C) Keep kit as submodule; open root `ENTRY.md`.

**简体中文：** (A) 将 **`en/` 或 `zh-CN/` 整树**复制到目标仓 `docs/`（含该树的 `global/`）。(B) 双语产品库：在 `docs/en/` 与 `docs/zh-CN/` 下镜像两棵树。(C) 套件作子模块：贡献者从根目录 `ENTRY.md` 进入。

Cross-links in templates assume the **locale folder** is the doc root / 模板内链假设**语言根目录**即文档根。

### 简体中文（概要）

上文英文已含技术细节表与目录树；**权威英文与中文语义一致**。维护时两棵树 `en/` 与 `zh-CN/` 文件名与章节结构应保持镜像；`global/` 两份文件内容建议保持同步。
