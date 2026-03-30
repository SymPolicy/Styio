# Contributor and automation specification / 贡献者与自动化规程

### English

**Purpose:** Rules for humans and agents: layout, workflow, tests, docs, prohibitions. Domain: `DOMAIN-OR-PRODUCT-SPEC.md`. Topology: `DOCUMENTATION-POLICY.md`. Globals: [`./global/`](./global/). **Version / Authority:** (set when forking) / effective once adopted.

**1. Repository overview** — Fill: languages, build, format/lint, tests, package manager.  
**2. Layout** — `docs/` holds `global/`, policy, spec, `history/`, `milestones/`, `tests/`; plus `<source>/`, `<tests>/`, CI config.  
**3. Change workflow** — Read domain spec + open questions; check milestone; implement + register tests; format; `history`; update SSOT.  
**4. Coding standards** — (project fills).  
**5. Testing** — Extend tests; don’t break without rationale; `TEST-CATALOG`.  
**6. Documentation** — Behavior changes → domain spec; SSOT; one canonical example if used.  
**7. Prohibited** — Unapproved deps; editing vendor/generated against policy; secrets; phantom tests; silent acceptance changes.  
**8. Authority** — Self-serve vs human approval vs conflict resolution (project fills).  
**Appendix** — Checklist: read policy + spec; build/test; change + catalog; history; review links.

### 简体中文

**文档作用：** 约束人类与自动化代理：仓库布局、变更流程、测试、文档与禁止项。**域语义**见 `DOMAIN-OR-PRODUCT-SPEC.md`；**文档拓扑**见 `DOCUMENTATION-POLICY.md`；**全局规约**见 [`./global/`](./global/)。**Version / Authority：** （分叉时填写）/ 采纳后生效。

**1. 仓库概览** — 填写语言、构建、格式化、测试、包管理。  
**2. 目录** — `docs/` 存 `global/`、策略与规格及 `history/`、`milestones/`、`tests/`；另有源码与测试与 CI。  
**3. 变更流程** — 读域规格与开放问题；核对里程碑；实现并登记测试；格式化；写 `history`；更新 SSOT。  
**4. 编码标准** — （项目填写）。  
**5. 测试** — 行为变更扩测；不得无说明破坏检查；维护 `TEST-CATALOG`。  
**6. 文档** — 用户可见行为变化更新域规格；遵守 SSOT；示例一处权威。  
**7. 禁止项** — 违规依赖；违规改 vendor/生成物；密钥；无登记自称有测；擅自改验收表。  
**8. 决策权限** — 自助/人批/冲突规则（项目填写）。  
**附录** — 核对清单与英文相同。
