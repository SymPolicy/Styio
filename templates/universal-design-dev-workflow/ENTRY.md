# Design-to-development workflow — entry guide / 设计—研发工作流入口

### English

**Purpose:** Onboarding for this kit: **how to run the workflow end-to-end**, where **global** norms live (**inside each locale tree**), and **copy-paste prompts** to bootstrap an assistant.

**Global conventions:** [`en/global/`](./en/global/) and [`zh-CN/global/`](./zh-CN/global/) are **duplicate** bilingual normative bundles—use the `global/` folder next to the tree you work in.

### 简体中文

**文档作用：** 本套件唯一编排入口：**端到端怎么用**、`global/` 在**各语言树内部**的位置，以及 **AI 一键启动提示词**。

**全局规约：** [`en/global/`](./en/global/) 与 [`zh-CN/global/`](./zh-CN/global/) 为**内容一致的两份**中英双语规约；请使用你当前所在语言根目录下的 `global/`。

---

## Repository layout / 本套件目录

### English

| Path | Role |
|------|------|
| [`ENTRY.md`](./ENTRY.md) | This file |
| [`en/`](./en/) | Locale tree A (with `en/global/`) |
| [`zh-CN/`](./zh-CN/) | Locale tree B (with `zh-CN/global/`) |

Pick **one** tree to copy into your product repo’s `docs/`, **including** its `global/` folder.

### 简体中文

| 路径 | 用途 |
|------|------|
| [`ENTRY.md`](./ENTRY.md) | 本文件 |
| [`en/`](./en/) | 语言树 A（内含 `global/`） |
| [`zh-CN/`](./zh-CN/) | 语言树 B（内含 `global/`） |

落地时任选 **一整树** 复制到产品仓 `docs/`，**须含**该树下的 `global/`。

---

## 简体中文 — 推荐阅读顺序

1. 打开所选语言树下的 [`global/README.md`](./zh-CN/global/README.md) 与 [`global/ARTIFACT-LAYOUT.md`](./zh-CN/global/ARTIFACT-LAYOUT.md)（`en/` 或 `zh-CN/` 路径相同）。  
2. 阅读同树 [`WORKFLOW-MAP.md`](./zh-CN/WORKFLOW-MAP.md)。  
3. **先填** [`OPEN-QUESTIONS-AND-HUMAN-INPUT.md`](./zh-CN/OPEN-QUESTIONS-AND-HUMAN-INPUT.md)。  
4. 起草 [`DOMAIN-OR-PRODUCT-SPEC.md`](./zh-CN/DOMAIN-OR-PRODUCT-SPEC.md) 与里程碑、测试目录（`milestones/`、`tests/`）。  
5. 将 [`CONTRIBUTOR-AND-AGENT-SPEC.md`](./zh-CN/CONTRIBUTOR-AND-AGENT-SPEC.md) 换成真实构建/测试命令。  
6. 按日写 [`history/`](./zh-CN/history/) 落地文件。

### 一键启动主提示词（中文指令 + English constraints）

```text
你是资深技术文档与交付流程教练。请严格按顺序工作，并遵守用户当前选定的语言根目录（en/ 或 zh-CN/）下 global/ 中的 ARTIFACT-LAYOUT.md 与 SSOT-AND-MAINTENANCE-RULES.md（若无法读盘，请用户粘贴）。

You must output bilingual Markdown (English + 简体中文 sections) when generating or editing any artifact in this kit.

0) 概括 ENTRY + WORKFLOW-MAP 的阶段与门禁（中英各一小段）。
1) 打开所选树下的 OPEN-QUESTIONS-AND-HUMAN-INPUT.md，列出 A–E 空项或 BLOCKING；缺信息则提问，勿编造。
2) 起草或补全 DOMAIN-OR-PRODUCT-SPEC.md（语境、术语、不变量、接口）；未知标 TBD / 待补充。
3) 依里程碑设想（无则自拟 M1…）写 00-Milestone-Index + 分段规格草稿；日期占位 YYYY-MM-DD。
4) 为验收写 TEST-CATALOG 示例行；无自动化标 gap；命令用占位。
5) 给出「下一步给人」清单（谁填开放问题、谁审域规格、谁改 CI 命令）。

约束 Constraints: No unrelated commercial names unless user gives them. SSOT — link authorities in DOCUMENTATION-POLICY; no long duplication.

用户上下文 User context:
<粘贴 / Paste: team, product type, constraints, commands, milestones, open issues>
```

### 分文档提示词

[`zh-CN/prompts/README.md`](./zh-CN/prompts/README.md)（与 `en/prompts/` 内容镜像相同）。

---

## English — Suggested reading order

1. Open [`global/README.md`](./en/global/README.md) and [`global/ARTIFACT-LAYOUT.md`](./en/global/ARTIFACT-LAYOUT.md) under **`en/`** (or `zh-CN/` if that is your working tree).  
2. Read [`WORKFLOW-MAP.md`](./en/WORKFLOW-MAP.md) in the same tree.  
3. Fill [`OPEN-QUESTIONS-AND-HUMAN-INPUT.md`](./en/OPEN-QUESTIONS-AND-HUMAN-INPUT.md).  
4. Author domain spec, milestones, test catalog.  
5. Replace placeholders in [`CONTRIBUTOR-AND-AGENT-SPEC.md`](./en/CONTRIBUTOR-AND-AGENT-SPEC.md).  
6. Maintain [`history/`](./en/history/) logs.

### Master bootstrap prompt (bilingual output required)

```text
You are a senior documentation and delivery coach. Follow the phase order in ENTRY / WORKFLOW-MAP. Obey `global/ARTIFACT-LAYOUT.md` and `global/SSOT-AND-MAINTENANCE-RULES.md` inside the user’s chosen locale root (`en/` or `zh-CN/`). Ask the user to paste those files if you cannot read the repo.

你必须对在本 kit 内生成或修改的每份文档输出中英双语段落（English + 简体中文）.

0) Summarize phases and gates (brief EN + 简短短文).
1) List OPEN_QUESTIONS blockers in A–E; do not invent facts.
2) Draft DOMAIN-OR-PRODUCT-SPEC; TBD / 待补充 for unknowns.
3) Draft milestone batch index + segments; YYYY-MM-DD placeholders.
4) Propose TEST-CATALOG rows; label gap; command placeholders.
5) Human checklist: owners for questions, spec review, CI commands.

Constraints: SSOT per DOCUMENTATION-POLICY; no long duplicate prose.

User context:
<Paste: team, product type, constraints, commands, milestones, open issues>
```

### Per-document prompts

[`en/prompts/README.md`](./en/prompts/README.md) (mirror of `zh-CN/prompts/`).

---

## After first bootstrap / 首次启动之后

### English

Resolve blocking open questions; file ADRs from `architecture/ADR.template.md`; keep domain spec, milestones, and test catalog consistent in one change set when semantics shift.

### 简体中文

收敛阻塞开放问题；用 `architecture/ADR.template.md` 写 ADR；语义变更时尽量同批更新域规格、里程碑验收与测试目录。
