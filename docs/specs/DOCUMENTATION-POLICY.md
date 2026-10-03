# Styio Documentation Policy

**Purpose:** Define where development Markdown belongs, how distributed and cross-feature SSOT references work, and how `docs/` metadata, indexes, and maintenance gates are enforced.

**Last updated:** 2026-10-01

**Automation:** Use `scripts/docs-index.py --check`,
`scripts/docs-lifecycle.py validate`, and `scripts/docs-audit.py` for document
structure. Run the focused language or tooling tests when a change affects its
contract, and record any unavailable acceptance separately. Test registration
and commands are owned by [TEST-CATALOG](../../workflows/TEST-CATALOG.md).

---

## 0. 文档维护准则（最小改动与单一事实来源）

### 0.1 Top-Level `Purpose`

Every `docs/**/*.md` file must expose top-level purpose metadata near the title. The accepted forms are:

- a single-line `**Purpose:** ...` metadata line; or
- for bilingual documents, an `[EN] Purpose: ...` line followed by a translated line such as `[CN] 目标：...`.

The purpose metadata should say when the reader should use the document and, when relevant, what the document does **not** own. Do not add new standalone metadata keys such as `文档作用：` outside the bilingual form above.

### 0.1.1 Top-Level `Last updated`

Every `docs/**/*.md` file must expose machine-readable update metadata near the title. The accepted forms are:

- a single-line `**Last updated:** YYYY-MM-DD`; or
- for bilingual documents, an `[EN] Last updated: YYYY-MM-DD` line followed by a translated line such as `[CN] 更新日期：YYYY-MM-DD`.

### 0.1.2 Documentation Language

Repository documentation is English by default. New or materially edited development docs, design docs, specs, ADRs, runbooks, workflows, plans, test catalogs, audit records, and source-level README files must be written in English.

Chinese or bilingual text is allowed only when the document is explicitly a user-facing product page, marketing page, localization artifact, or translation companion, such as the root `README.md` / `README_zh.md` pair. When a document needs Chinese text, keep the English source authoritative and label the translated section or companion file clearly.

When touching an existing non-English development document, do not add more non-English maintenance prose. Convert the edited section to English unless the file has an explicit localization purpose.

### 0.2 最小改动原则

- 增补约定时 **优先** 修改现有章节并加交叉链接，**非必要不新增** Markdown 文件。
- 若内容可并入现有权威文档（如下表），则不应另立平行长文。
- 新建 Markdown 前必须先搜索生成索引、`docs/adr/`、owning SSOT、team runbook、workflow、plan 与 rollup；只有确认现有文档不能承载时，才允许创建单一职责（single responsibility）的新文档。
- 使用 `scripts/docs-scaffold.py` 创建文档时必须传 `--reuse-reviewed`，表示已经完成上述搜索并确认不是重复文档。
- 一个功能不允许拆成多个平行解释文档；除非各文件分别拥有不同 artifact class，例如 design SSOT、test catalog、team runbook、workflow 或 external handoff。
- ADR 记录当前决策，不记录旧决策与新决策的并置历史。若已有 active ADR 覆盖同一决策边界，更新原 ADR 的当前 `Decision`；旧文本由 Git history 提供。

Distributed syntax-feature SSOT documents are the intentional exception to
the one-feature/one-explanation warning: each document owns one durable
feature boundary, while shared grammar, token, and semantic invariants remain
in cross-feature design documents.

### 0.3 「三处规则」：去重后再引用

若 **三篇及以上** 文档对**同一细节或同一功能**作出**实质性解释**（不仅是「详见某某」式单向链接），则必须：

1. 指定 **唯一权威（SSOT）** —— **优先**选用下表或已有文档中的某一节；
2. 仅在确无合适承载处时，才新增专题短文；
3. 其余文档 **删除或缩编** 重复段落，改为 **链接 SSOT**。

### 0.4 常见单一事实来源（SSOT）速查

| 主题 | 权威文档 | 其它文档应 |
|------|----------|------------|
| Feature-specific syntax, semantics, lifecycle, dependencies, prerequisites, and evidence map | `../design/syntax/features/<feature-id>.md` | Update the owning feature SSOT first; consume the generated graph only as a composed view |
| Language design requirements and capability traceability | `../design/Styio-Language-Design.md` sections 2.4–2.5 | Keep one embedded `toml design-intent` block; it links exact semantic/wire authorities and evidence, and never defines a second grammar |
| Cross-feature semantic principles and section structure | `../design/Styio-Language-Design.md` | Link to the shared invariant; do not duplicate feature lifecycle facts |
| Composed lexical and grammar EBNF | `../design/Styio-EBNF.md` | Link; the owning feature SSOT records why the rule applies |
| Shared symbol ↔ lexer token names | `../design/Styio-Symbol-Reference.md` | Link; the owning feature SSOT records the feature boundary |
| `@` 拓扑目标语法、Golden Cross **设计级**叙述与示例形态 | `../design/Styio-Resource-Topology.md`（含 §8） | 保留链接或一句摘要 |
| 当前实现缺口与跨团队排期 | `../rollups/NEXT-STAGE-GAP-LEDGER.md` | 链接，不另建平行 backlog |
| 集成测试路径、`ctest` 命令 | `../../workflows/TEST-CATALOG.md` | 链接 |
| **外部包 / 开源依赖清单**（LLVM、ICU、gtest、vendored） | [`THIRD-PARTY.md`](./THIRD-PARTY.md) | 与 `CMakeLists.txt`、`tests/CMakeLists.txt` 一致；新增依赖先更新该文件 |
| **官方仓库生态、角色边界与文档归属** | [`REPOSITORY-MAP.md`](./REPOSITORY-MAP.md) | 其它文档只链接，不重复维护仓库总表 |
| **团队日常工作入口、review 协作矩阵与维护者 runbook** | [`../teams/COORDINATION-RUNBOOK.md`](../teams/COORDINATION-RUNBOOK.md) | 团队文档只做日常入口，语言/测试/仓库边界仍链接 owning SSOT |
| **项目级原则与目标**（规划 / 设计 / 开发 / 测试 / 审核的优先级） | [`PRINCIPLES-AND-OBJECTIVES.md`](./PRINCIPLES-AND-OBJECTIVES.md) | 其它文档引用，不平行重写项目级优先级与重写边界 |
| **默认冷启动摘要 / 当前仓库状态** | [`../rollups/CURRENT-STATE.md`](../rollups/CURRENT-STATE.md) | 先读本文件，再跳到 owning SSOT |
| **五层编译流水线** goldens（Lexer/IR/…） | `../../workflows/FIVE-LAYER-PIPELINE.md` | 与 `TEST-CATALOG` §9 交叉链接 |
| 开发文档目录与维护准则（含本节） | `DOCUMENTATION-POLICY.md` | 链接 |
| Agent 实现规程、禁止项、流水线 | `AGENT-SPEC.md` | 链接 |
| Golden Cross **守则内嵌的宪法示例代码** | `AGENT-SPEC.md` §12.3 | 设计背景链到 `../design/Styio-Resource-Topology.md` §8 |
| resource topology **设计、实现状态与迁移入口** | `../design/Styio-Resource-Topology.md` + `../rollups/NEXT-STAGE-GAP-LEDGER.md` | 不保留平行长计划 |
| **Checkpoint 执行规则**（可中断/可恢复） | `../../workflows/CHECKPOINT-WORKFLOW.md` | 在 `history/YYYY-MM-DD.md` 写恢复指引，不在其它文档重复流程细节 |
| **统一交付门禁**（common delivery floor） | `../../workflows/DELIVERY-GATE.md` | 先过 common floor，再按协调 runbook 叠加域专属 cutover gate |
| **新语法添加工作流**（含 runtime helper / ORC 注册对齐） | `../../workflows/ADD-SYNTAX-WITH-SKILLS.md` | 前端、Codegen/Runtime、测试与 docs 只保留入口规则与链接 |
| **语法契约纠正工作流**（用户质疑 / parser-Sema-EBNF 不一致） | `../../workflows/CORRECT-SYNTAX-CONTRACT.md` | 智能体先读 workflow，再改 parser、Sema、lowering、测试或语法文档 |
| **工作流调度与分离原则** | `../../workflows/WORKFLOW-ORCHESTRATION.md` | 新增 workflow / gate 前必须先查该表；工具调用顺序由 `scripts/workflow-scheduler.py` 固化 |
| **仓库清理、提交、push 与历史重写标准** | `../../workflows/REPO-HYGIENE-COMMIT-STANDARD.md` | 其它文档只保留入口规则与链接 |
| **文档元数据、生成索引与审计流程** | `../../workflows/DOCS-MAINTENANCE-WORKFLOW.md` | 其它文档只保留入口规则与链接 |
| **团队 runbook 维护交付门禁** | `../../workflows/TEAM-RUNBOOK-MAINTENANCE-GATE.md` | `docs-audit.py` 串联该门禁；团队文档只链接门禁说明与模板 |
| **团队 runbook 标准格式** | `../assets/templates/TEAM-RUNBOOK-TEMPLATE.md` | 普通团队 runbook 必须使用该 H2 结构；协调者 runbook 的特殊结构由门禁说明列明 |
| **架构决策 provenance（非活跃 SSOT）** | `docs/adr/IMPLEMENTED-DECISIONS.md`、Git history | `IMPLEMENTED-DECISIONS.md` 只保留当前最新代码实现对应的压缩决策；活跃规则必须提升到 owning SSOT；旧 ADR 全文和被替换实现使用 Git history |

### 0.5 Active State and Historical Provenance

1. Current maintenance knowledge belongs in design/specs/teams, root workflows,
   current rollups, and the owning active plan.
2. The registered [plan workspace](../plan/README.md#state-ownership) owns semantic
   delivery state, execution checkpoints, and generated projections. Its tracked
   completed delivery records remain part of that registry. Remove superseded
   standalone planning narratives after their durable content is absorbed;
   do not delete registered records solely because a stage completed.
3. Language acceptance belongs in feature contracts and executable tests, with
   the test catalog indexing evidence. Plans sequence work rather than defining
   a second language specification.
4. Active syntax distinguishes canonical teaching forms, accepted compatibility
   forms, reserved proposals and rejected forms. Keep compatibility rationale
   and executable evidence with the owning feature.
5. Resolve duplicate draft/active specifications at their source. Promote valid
   decisions and open questions to the owner before removing absorbed copies.
6. ADR/history/archive are provenance or recovery surfaces. Maintain a current
   decision record while it serves active review; use Git history for replaced
   decisions and exact old prose. The implemented-decision summary links current
   authorities and does not become an additional feature backlog.
7. A historical test result remains tied to its recorded revision and run. Do
   not restate it as current acceptance without fresh verification.

### 0.6 文档目录职责

| 路径 | 存放内容 |
|------|----------|
| `docs/design/` | 语言设计、EBNF、符号表、资源/标准库等设计级 SSOT |
| `docs/specs/` | agent / contributor 规范、文档策略、依赖规范 |
| `docs/teams/` | 团队日常 runbook、review 协作矩阵、跨团队维护入口；不替代语言、测试或仓库边界 SSOT |
| `docs/review/` | review 发现、设计冲突、待定决议；不保留已归纳的旧 dated bundle |
| `docs/plan/` | Registered semantic plans, execution checkpoints and generated delivery records; state ownership is defined by `docs/plan/README.md` |
| `docs/external/for-ide/` | IDE 集成、LSP 调用、嵌入方式与 edit-time 语法层使用说明 |
| `docs/assets/templates/` | 可复用模板 |
| 根目录 `workflows/` | 可复用工作流、测试框架、checkpoint / hygiene 标准（机器可读 `*.toml` 配对，repo-local skills 在 `workflows/skills/`） |
| `docs/rollups/` | 压缩后的 active 摘要；默认冷启动先读这里 |
| `docs/archive/` | 最小 lifecycle metadata 壳；不保留旧语法目录、旧示例、旧 source、历史 plan/rollup 快照 |
| `docs/history/` | 恢复入口；默认不保留 raw dated checkpoint，精确历史文本使用 Git history |
| `docs/adr/` | 尚未吸收到主文档或仍需单独审计追溯的决策记录；吸收后的旧文本依赖 Git history 追溯 |

### 0.7 文件命名约定

0. Feature, module, workflow, skill, and documentation transformations must not use version-style names such as `v2`, `version`, `new`, `old`, `legacy`, or `latest`; name the artifact by the feature or transformation result so the active tree has one current implementation and no renamed old/new residue.
0. Repo-owned documentation and skills must not expose developer-machine paths, server-machine paths, private endpoints, account names, hostnames, or deployment roots. Use placeholders such as `<workspace-root>`, `<user-home>`, `<server-host>`, `<service-url>`, `<private-ip>`, or documented environment variables, and run `python3 scripts/local-info-leak-gate.py --mode worktree` before delivery.
1. `docs/design/`：设计级 SSOT 使用稳定、可搜索的主题名；当前约定为 `Styio-*.md`。
2. `docs/specs/`：规范文件使用稳定、可搜索的全大写短横线命名。
3. `docs/teams/`：团队日常入口使用 `<TEAM>-RUNBOOK.md`；跨团队协调入口固定为 `COORDINATION-RUNBOOK.md`；集合统计固定为 `DOC-STATS.md`。
   普通团队 runbook 必须遵守 `docs/assets/templates/TEAM-RUNBOOK-TEMPLATE.md` 的 H1、`Purpose`、`Last updated`、H2 顺序；交付门禁输出必须指向模板和门禁说明，而不是只要求维护者阅读脚本源码。
4. `docs/plan/`：计划文件必须使用描述性名称，优先 `<Topic>-Plan.md`、`<Topic>-Implementation-Plan.md`、`<Topic>-Adjustment.md`；禁止再新增 `idea.md`、`notes.md`、`misc.md` 这类泛名文件。
5. 根 `workflows/` 与 `docs/assets/templates/`：可复用资产采用稳定、可搜索的全大写短横线命名。
6. `docs/history/`：严格使用 `YYYY-MM-DD.md`。
7. `docs/adr/`：严格使用 `ADR-XXXX-<slug>.md`。

### 0.8 Directory Entry Rules

1. Every collection directory under `docs/` must provide both `README.md` and `INDEX.md`.
2. `README.md` owns **scope, naming, and maintenance rules** only.
3. `INDEX.md` owns the **generated inventory** for that directory.
4. `README.md` must point readers to `INDEX.md`; it should not duplicate a full file inventory.
5. Adding a new top-level collection directory requires updating `docs/README.md`, this policy, and the docs-index generator configuration.

### 0.9 Generated Index Rules

1. Collection-directory `INDEX.md` files are generated by `python3 scripts/docs-index.py --write`.
2. Generated indexes must not be hand-maintained.
3. Structural validation runs through `python3 scripts/docs-audit.py`, `ctest --test-dir build/default -L docs`, and the `checkpoint-health` workflow.
4. A docs-tree change is not complete until the generated indexes and docs audit both pass.
5. `docs-audit.py` validates the Language Design intent block as a closed TOML metadata schema, including required principles, scoped implementation states, owning runbooks, source/test evidence, and Markdown section links. `tests/design_intent_contract_test.py` supplies positive and negative regression evidence; semantic adequacy still requires review.

### 0.10 Repo-Wide Markdown Manifest

`scripts/docs-audit.py` also owns the repository-wide Markdown inventory used to answer two questions:

1. Which Markdown files are **valid repository documents** and should stay discoverable?
2. Which Markdown files are **invalid or out-of-scope** and should be reviewed for deletion, relocation, or de-tracking?

The default manifest source is **worktree Markdown that is tracked or unignored**. This keeps newly added docs visible before `git add`, while still excluding ignored build output and local report directories. Use `--source git` when you want strictly tracked files only, and `--source filesystem` only when you intentionally want to inspect ignored local build output, generated reports, or other Markdown currently present in the worktree.

Approved repository-document locations are:

- root `README.md`
- `docs/**/*.md`
- `benchmark/**/*.md` only for Styio probe/adaptor documentation; benchmark workloads, reports, baselines, and regression records belong in `styio-benchmark`
- `templates/**/*.md`
- `grammar/tree-sitter-styio/README.md`
- `tests/**/README.md` and approved test templates such as `tests/**/REGRESSION-TEMPLATE.md`

Inventory commands:

```bash
python3 scripts/docs-audit.py --manifest valid --format tree
python3 scripts/docs-audit.py --manifest valid --format json --output /tmp/styio-docs.json
python3 scripts/docs-audit.py --manifest invalid --format list
python3 scripts/docs-audit.py --manifest invalid --format list --source filesystem
```

Manifest exports also include text-volume statistics for the selected document set:

- `character_count` uses the raw character length of each Markdown file.
- `word_count` uses a repository-local approximation rule: each Han character counts as 1, each contiguous ASCII word counts as 1, and each non-whitespace symbol counts as 1.

### 0.11 Time-Sensitive Doc Compression And Archive Lifecycle

1. First-wave time-sensitive families are:
   - `docs/history/*.md`
   - dated review bundles under `docs/review/<YYYY-MM-DD>/`
2. `docs/rollups/` is the active compression layer. It keeps concise current summaries and should be read before Git history or archive lifecycle metadata.
3. `docs/archive/` is a minimal lifecycle metadata shell, not a retention area for old syntax catalogs, archived examples, old source snapshots, old plans, or old rollups.
4. The JSON source of truth is `docs/archive/ARCHIVE-MANIFEST.json`; the human-facing generated view is `docs/archive/ARCHIVE-LEDGER.md`.
5. `python3 scripts/docs-lifecycle.py mark ...` records that a raw doc has been summarized into active docs. With the default zero keep window, its status becomes `pending_archive`.
6. `python3 scripts/docs-lifecycle.py cleanup ...` removes pending raw docs from active history/review locations after their durable value has been promoted.
7. Exact old raw text is recovered from Git history. Provenance, targets, and status must live in the manifest/ledger rather than being injected back into old raw file bodies.
8. `python3 scripts/docs-lifecycle.py validate` is a required gate. `docs-audit.py` calls it automatically.
9. Relative-link freshness is enforced for active docs. Historical prose recovered from Git history is not an active-doc link-normalization target.

### 0.12 Public Wording Discipline

1. Repository documentation must stay concise, rigorous, and evidence-scoped.
2. Do not speculate about companies, organizations, individuals, projects, products, or their capabilities.
3. External systems may be named only as cited references, integration targets, or measured baselines with reproducible evidence.
4. Avoid absolute marketing superlatives and unsupported superiority language. Use neutral terms such as "reference", "baseline", "measured result", or "implementation target".
5. Performance, safety, resource-management, and maturity statements must point to compiler tests, `styio-benchmark` reports, audit records, or primary source references.
6. Neutral uses of `claim`, its inflected forms, and `performance claims` are allowed. These terms alone do not establish unsupported superiority; assess the actual statement and its evidence under the rules above.

---

## 1. Current Documentation

- Keep accepted behavior, maintenance procedures and current limitations in the
  owning active document. Revise obsolete statements instead of appending
  contradictory updates.
- Keep reusable test instructions in [TEST-CATALOG](../../workflows/TEST-CATALOG.md),
  with current input/oracle paths and reproducible CTest selections.
- Record implementation status separately from proposed behavior and dated
  validation evidence. A previous test count or failure rate is not a current run.
- Keep exact historical prose in Git. A completed change does not require a
  permanent duplicate daily log after its durable content has been absorbed.

---

## 2. Recovery Notes

Use `docs/history/YYYY-MM-DD.md` only when an interrupted checkpoint needs
recovery information that is not already represented in its owning plan or
active handoff. Include purpose, update date, state, next action, reproducible
commands, unverified gates and rollback reference.

After the checkpoint closes, promote current decisions and procedures to their
owner documents and unresolved work to the gap ledger. Register the extracted
value and targets with `docs-lifecycle.py mark`, then run `cleanup` under the
zero-retention policy. This cleanup removes active copies; it does not rewrite
Git history or erase an unresolved obligation.

`docs/history/README.md` owns scope and recovery instructions. Its `INDEX.md` is
generated by `docs-index.py`; neither file is a manually maintained daily diary.

---

## 3. 语言特性测试文档

| 规则 | 说明 |
|------|------|
| 目录 | 所有语言验收用例按语言特性放在 `tests/features/<feature>/`。 |
| 文档入口 | `../../workflows/TEST-CATALOG.md` 是测试目录、CTest label、特殊 gate 的唯一活跃索引。 |
| 与实现关系 | 新增或移动 `.styio` fixture 时，必须同步更新 `tests/CMakeLists.txt`、feature catalog、受影响 team runbook。 |

---

## 4. 测试目录 `workflows/TEST-CATALOG.md`

| 规则 | 说明 |
|------|------|
| 划分维度 | **按语言功能域**，而非按历史编号或内部文件名。 |
| 每条目 | 至少包含：**CTest 名**、**输入**（`.styio` 路径）、**输出/Oracle**（`expected/*.out` 或文档约定的临时文件路径）、**自动化**（`ctest -R '…'` 或 `ctest -L …`）。 |
| 与构建一致 | 新增 `.styio` 验收测试时，必须同时更新 `tests/CMakeLists.txt`（或项目约定的单一注册处）与 `../../workflows/TEST-CATALOG.md`。 |

单条示例（字段名固定，便于将来脚本解析）：

| CTest | Input | Oracle | Automation |
|-------|-------|--------|------------|
| `scalar_expressions_t01_int_arith` | `tests/features/scalar_expressions/t01_int_arith.styio` | `tests/features/scalar_expressions/expected/t01_int_arith.out` | `ctest --test-dir build/default -R '^scalar_expressions_t01_int_arith$'` |

---

## 5. 与 `AGENT-SPEC.md` 的关系

语言与编译器实现规范仍以 [`AGENT-SPEC.md`](./AGENT-SPEC.md) 为准；**文档存放位置、history/checkpoint/feature-test 目录约定及 §0 维护准则** 以本文件为准。二者冲突时，先更新本策略与索引，再改 `AGENT-SPEC` 中的引用。

---

## 6. Automation Gates

1. `python3 scripts/docs-index.py --check` verifies generated collection indexes.
2. `python3 scripts/docs-lifecycle.py validate` verifies lifecycle metadata,
   extraction targets, retention and source/archive state.
3. `python3 scripts/docs-audit.py` verifies metadata, naming, links, directory
   entrypoints and embedded documentation contracts.
4. `python3 scripts/docs-audit.py --manifest invalid --format list` identifies
   out-of-scope worktree documents; use `--source filesystem` when intentionally
   inspecting ignored build output.
5. Run focused CTest selections from `workflows/TEST-CATALOG.md` when language
   behavior or executable examples are affected. Use `--output-on-failure
   --no-tests=error`; a tolerated, failed or unexecuted test is not a pass.
6. Run the [documentation maintenance workflow](../../workflows/DOCS-MAINTENANCE-WORKFLOW.md)
   and applicable delivery gate. Missing platform, external audit or consumer
   evidence remains explicit in the handoff.

CMake-registered CTest and `styio --file` remain the executable language
acceptance entrypoints, as defined in `tests/CMakeLists.txt`.
