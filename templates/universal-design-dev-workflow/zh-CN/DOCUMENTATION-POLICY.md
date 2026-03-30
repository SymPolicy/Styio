# Documentation policy / 文档策略

### English

**Purpose:** Where Markdown lives, how **SSOT** works, and how history, milestones, and tests align with automation—not product semantics (`DOMAIN-OR-PRODUCT-SPEC.md`). **Normative:** [`./global/SSOT-AND-MAINTENANCE-RULES.md`](./global/SSOT-AND-MAINTENANCE-RULES.md). **Last updated:** (set when forking).

**Section 0 — Principles**  
**0.1 Purpose line:** State early why readers open the file and what it excludes.  
**0.2 Minimal change:** Prefer extending with links.  
**0.3 Three-or-more:** Deduplicate; one SSOT; link elsewhere.  
**0.4 SSOT quick reference:**

| Topic | Authority | Others should |
|-------|-----------|---------------|
| Product/domain | `DOMAIN-OR-PRODUCT-SPEC.md` | Link; no long repeat |
| Dependencies | `THIRD-PARTY.md` | Match lockfiles |
| Milestone batch | `milestones/<YYYY-MM-DD>/00-Milestone-Index.md` | No forked master table |
| ADR | `architecture/ADR-NNNN-*.md` | Link only |
| Tests | `tests/TEST-CATALOG.md` | One row + command |
| Doc layout | This file | Link here |
| Contributors | `CONTRIBUTOR-AND-AGENT-SPEC.md` | Link here |
| Open questions | `OPEN-QUESTIONS-AND-HUMAN-INPUT.md` | Link until closed |

**Section 1 — History:** `history/YYYY-MM-DD.md`; index `history/README.md`.  
**Section 2 — Milestones:** `milestones/<YYYY-MM-DD>/`; files `00-Milestone-Index.md`, `M*.md`; acceptance in catalog or **gap**.  
**Section 3 — Test catalog:** By area; id, input, oracle, automation; manifest + catalog.  
**Section 4 — Contributor spec:** On conflict, update **this policy first**.  
**Section 5 — Optional checks:** Tests pass; `Last updated`; paths real or pending.

### 简体中文

**文档作用：** 约定开发类 Markdown 存放、**单一事实来源**及历史/里程碑/测试与自动化对齐；**不含**产品语义（见 `DOMAIN-OR-PRODUCT-SPEC.md`）。**权威条款：** [`./global/SSOT-AND-MAINTENANCE-RULES.md`](./global/SSOT-AND-MAINTENANCE-RULES.md)。**Last updated：** （分叉时填写）。

**第 0 节 — 原则：** 文首「文档作用」；最小改动；三处去重；上表为 SSOT 速查（与英文表一致）。

**第 1 节 — 历史：** `history/YYYY-MM-DD.md`；索引 `history/README.md`。

**第 2 节 — 里程碑：** `milestones/<YYYY-MM-DD>/`；`00-Milestone-Index.md` 与各 `M*.md`；验收须进目录或标 **gap**。

**第 3 节 — 测试目录：** 按功能域；登记构建/CI 与目录双线。

**第 4 节 — 贡献者规程：** 冲突时**先**更新本策略。

**第 5 节 — 可选校验：** 测试；`Last updated`；路径。
