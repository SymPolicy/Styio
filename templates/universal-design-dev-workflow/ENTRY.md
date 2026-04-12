# Design-to-development workflow — entry guide / 设计—研发工作流入口

### English

**Purpose:** How to run the workflow end-to-end, where **`global/`** lives (**inside** `en/` or `zh-CN/`), and **copy-paste prompts** for an assistant.

**Locale rule:** [`en/`](./en/) Markdown is **English-only**; [`zh-CN/`](./zh-CN/) Markdown is **简体中文 only** (prose). Each tree includes its own [`global/`](./en/global/) norms in that same language.

### 简体中文

**文档作用：** 编排说明、`global/` 位于 **`en/` 或 `zh-CN/` 树内**、以及 **AI 启动提示词**。

**语言规则：** [`en/`](./en/) 下正文**仅英文**；[`zh-CN/`](./zh-CN/) 下正文**仅简体中文**。各树自带同语言的 `global/`。

---

## Repository layout / 套件目录

### English

| Path | Role |
|------|------|
| [`ENTRY.md`](./ENTRY.md) | This file (bilingual navigator) |
| [`en/`](./en/) | English-only templates + `global/` + `prompts/` |
| [`zh-CN/`](./zh-CN/) | Chinese-only templates + `global/` + `prompts/` |

Copy **one** full tree (`en/` **or** `zh-CN/`), **including** its `global/`, into your product `docs/` when adopting.

### 简体中文

| 路径 | 用途 |
|------|------|
| [`ENTRY.md`](./ENTRY.md) | 本文件（根目录双语导航） |
| [`en/`](./en/) | 英文模板 + `global/` + `prompts/` |
| [`zh-CN/`](./zh-CN/) | 中文模板 + `global/` + `prompts/` |

落地时将 **整棵** `en/` **或** `zh-CN/`（**含**其 `global/`）复制到产品仓 `docs/`。

---

## 简体中文 — 阅读顺序与主提示词

1. [`zh-CN/global/README.md`](./zh-CN/global/README.md)、[`zh-CN/global/ARTIFACT-LAYOUT.md`](./zh-CN/global/ARTIFACT-LAYOUT.md)  
2. [`zh-CN/WORKFLOW-MAP.md`](./zh-CN/WORKFLOW-MAP.md)  
3. [`zh-CN/OPEN-QUESTIONS-AND-HUMAN-INPUT.md`](./zh-CN/OPEN-QUESTIONS-AND-HUMAN-INPUT.md)  
4. [`zh-CN/DOMAIN-OR-PRODUCT-SPEC.md`](./zh-CN/DOMAIN-OR-PRODUCT-SPEC.md)、里程碑、测试目录  
5. [`zh-CN/CONTRIBUTOR-AND-AGENT-SPEC.md`](./zh-CN/CONTRIBUTOR-AND-AGENT-SPEC.md) 换成真实命令  
6. 按日维护 [`zh-CN/history/`](./zh-CN/history/) 落地文件  

**主提示词（工作在 `zh-CN/` 树时；输出须为简体中文）：**

```text
你是资深技术文档与交付流程教练。用户在维护 zh-CN/ 语言树。请遵守该树下 global/ARTIFACT-LAYOUT.md 与 global/SSOT-AND-MAINTENANCE-RULES.md（无法读盘时请用户粘贴）。

规则：在 zh-CN/ 内生成或修改的 Markdown 叙述部分必须为简体中文；代码、命令、标识符可与原文一致。

0) 概括 ENTRY 与本树 WORKFLOW-MAP 的阶段与门禁（用中文）。
1) 打开 zh-CN/OPEN-QUESTIONS-AND-HUMAN-INPUT.md，列出 A–E 空项或 BLOCKING；缺信息则提问，勿编造。
2) 起草或补全 zh-CN/DOMAIN-OR-PRODUCT-SPEC.md；未知标「待补充」。
3) 依里程碑设想写 00-Milestone-Index 与分段草稿；日期占位 YYYY-MM-DD。
4) 为验收写 TEST-CATALOG 示例行；无自动化标 gap。
5) 给出「下一步给人」清单。

约束：勿编造商业产品名（除非用户提供）；遵守 SSOT，长篇事实只在一处权威文档叙述，他处链接。

用户上下文：
<团队、产品类型、硬约束、命令、里程碑、未决问题>
```

分文档提示词：[`zh-CN/prompts/README.md`](./zh-CN/prompts/README.md)

---

## English — Reading order and master prompt

1. [`en/global/README.md`](./en/global/README.md), [`en/global/ARTIFACT-LAYOUT.md`](./en/global/ARTIFACT-LAYOUT.md)  
2. [`en/WORKFLOW-MAP.md`](./en/WORKFLOW-MAP.md)  
3. [`en/OPEN-QUESTIONS-AND-HUMAN-INPUT.md`](./en/OPEN-QUESTIONS-AND-HUMAN-INPUT.md)  
4. [`en/DOMAIN-OR-PRODUCT-SPEC.md`](./en/DOMAIN-OR-PRODUCT-SPEC.md), milestones, test catalog  
5. [`en/CONTRIBUTOR-AND-AGENT-SPEC.md`](./en/CONTRIBUTOR-AND-AGENT-SPEC.md)  
6. [`en/history/`](./en/history/)

**Master prompt (when working in `en/`; output must be English prose):**

```text
You are a senior documentation and delivery coach. The user works in the en/ locale tree. Obey that tree’s global/ARTIFACT-LAYOUT.md and global/SSOT-AND-MAINTENANCE-RULES.md (ask the user to paste if unreadable).

Rule: Narrative Markdown under en/ must be English-only; code, commands, and identifiers may stay as needed.

0) Summarize phases and gates from ENTRY and this tree’s WORKFLOW-MAP.
1) List empty or BLOCKING rows in en/OPEN-QUESTIONS-AND-HUMAN-INPUT.md sections A–E; do not invent facts.
2) Draft or extend en/DOMAIN-OR-PRODUCT-SPEC.md; use TBD for unknowns.
3) Draft 00-Milestone-Index and segment specs; YYYY-MM-DD placeholders.
4) Propose TEST-CATALOG rows; label gap; command placeholders.
5) Human checklist: owners, spec review, CI updates.

Constraints: SSOT per DOCUMENTATION-POLICY; no long duplicate prose; no unrelated commercial names unless supplied.

User context:
<team, product type, constraints, commands, milestones, open issues>
```

Per-document prompts: [`en/prompts/README.md`](./en/prompts/README.md)

---

## After first bootstrap / 启动之后

### English

Resolve blockers; add ADRs from `architecture/ADR.template.md` in the same locale tree; co-update domain spec, milestones, and test catalog when semantics change.

### 简体中文

收敛阻塞项；在同一语言树内用 `architecture/ADR.template.md` 写 ADR；语义变更时同批更新域规格、里程碑与测试目录。
