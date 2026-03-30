# SSOT and maintenance rules / SSOT 与维护规则（规范性）

### English

**Purpose:** Language-independent documentation hygiene. Every `DOCUMENTATION-POLICY.md` in a locale tree should stay aligned with this file unless the project overrides in writing.

**Document purpose line:** Early in each file, state **why** readers open it and **what** it excludes.

**Minimal change:** Extend sections and link before adding parallel long documents.

**Three-or-more rule:** If **three or more** documents substantially repeat the same fact: (1) designate one SSOT, (2) add a new doc only if none fits, (3) deduplicate and link.

**SSOT slots (customize when forking):**

| Topic | Authority | Others should |
|-------|-----------|---------------|
| Product/domain | `DOMAIN-OR-PRODUCT-SPEC.md` | Link; short summary only |
| Dependencies | `THIRD-PARTY.md` | Match lockfiles/manifests |
| Milestone batch | `milestones/<YYYY-MM-DD>/00-Milestone-Index.md` | No ad hoc duplicate tables |
| Architecture | `architecture/ADR-NNNN-*.md` | Link only |
| Tests | `tests/TEST-CATALOG.md` | One row per check + command |
| Doc topology | `DOCUMENTATION-POLICY.md` | Link here |
| Contributors | `CONTRIBUTOR-AND-AGENT-SPEC.md` | Link here |
| Open questions | `OPEN-QUESTIONS-AND-HUMAN-INPUT.md` | Link until closed |

**History:** `history/YYYY-MM-DD.md`; index in `history/README.md`.  
**Milestones:** `milestones/<YYYY-MM-DD>/`, `00-Milestone-Index.md` + `M*.md`; acceptance names `TEST-CATALOG` rows or **gap**.  
**Test catalog:** By functional area; register in **one** CI/build manifest **and** catalog.  
**Conflict:** If policy vs contributor spec disagree, fix **policy/indexes first**.

**Optional checks:** Tests pass; `Last updated` present; catalog paths exist or marked pending.

### 简体中文

**文档作用：** 与具体自然语言无关的文献卫生规则；各语言树中的 `DOCUMENTATION-POLICY.md` 须与本文件一致，除非项目书面另有约定。

**「文档作用」行：** 文首说明阅读理由与不涵盖范围。  
**最小改动：** 能加链不加平行长文。  
**三处去重：** 三篇及以上实质性重复同一事实时：指定唯一 SSOT；无合适承载再增新文；其余删改并链接。

**SSOT 槽位（分叉时自定义路径）：** 见上表（与英文表一致）。

**历史目录：** `history/YYYY-MM-DD.md`；索引 `history/README.md`。  
**里程碑：** `milestones/<YYYY-MM-DD>/`；验收须对应 `TEST-CATALOG.md` 或标 **gap**。  
**测试目录：** 按功能域；构建/CI 清单与目录**双登记**。  
**冲突：** `DOCUMENTATION-POLICY.md` 与 `CONTRIBUTOR-AND-AGENT-SPEC.md` 不一致时，**先改策略与索引**。

**可选校验：** 测试通过；索引含 `Last updated`；目录路径存在或标待定。
