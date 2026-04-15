# Styio Documentation Policy

**Purpose:** Define where development Markdown belongs, how SSOT references work, and how `docs/` metadata, indexes, and maintenance gates are enforced; language semantics still live in `../design/Styio-Language-Design.md` and related design documents.

**Last updated:** 2026-04-10

**Automation (verify doc links + test registration):** 从仓库根目录配置并运行里程碑测试：

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build -L milestone
```

（GoogleTest 目标 `styio_test` 需单独构建；若本机 LLVM 与 libstdc++ 头文件冲突导致 gtest 编译失败，仍以 `styio` 可执行文件与 `ctest -L milestone` 为准。）

---

## 0. 文档维护准则（最小改动与单一事实来源）

### 0.1 Top-Level `Purpose`

Every `docs/**/*.md` file must expose a top-level **`Purpose:`** line near the title. The line should say when the reader should use the document and, when relevant, what the document does **not** own. Do not add new `文档作用：` metadata; `Purpose:` is the only accepted metadata key.

### 0.2 最小改动原则

- 增补约定时 **优先** 修改现有章节并加交叉链接，**非必要不新增** Markdown 文件。
- 若内容可并入现有权威文档（如下表），则不应另立平行长文。

### 0.3 「三处规则」：去重后再引用

若 **三篇及以上** 文档对**同一细节或同一功能**作出**实质性解释**（不仅是「详见某某」式单向链接），则必须：

1. 指定 **唯一权威（SSOT）** —— **优先**选用下表或已有文档中的某一节；
2. 仅在确无合适承载处时，才新增专题短文；
3. 其余文档 **删除或缩编** 重复段落，改为 **链接 SSOT**。

### 0.4 常见单一事实来源（SSOT）速查

| 主题 | 权威文档 | 其它文档应 |
|------|----------|------------|
| 语言语义与章节结构 | `../design/Styio-Language-Design.md` | 链到章节，避免大段复述 |
| 词法与文法 EBNF | `../design/Styio-EBNF.md` | 链接 |
| 符号 ↔ lexer token 名 | `../design/Styio-Symbol-Reference.md` | 链接 |
| `@` 拓扑目标语法、Golden Cross **设计级**叙述与示例形态 | `../design/Styio-Resource-Topology.md`（含 §8） | 保留链接或一句摘要 |
| 设计 / 实现冲突与待定决议 | `../review/Logic-Conflicts.md` | 链接 |
| M1–M7 路线图、依赖链、规格文件表 | `docs/milestones/<日期>/00-Milestone-Index.md` | **勿**在 history 等处平行维护同一张总表 |
| 集成测试路径、`ctest` 命令 | `docs/assets/workflow/TEST-CATALOG.md` | 链接 |
| **外部包 / 开源依赖清单**（LLVM、ICU、gtest、vendored） | [`THIRD-PARTY.md`](./THIRD-PARTY.md) | 与 `CMakeLists.txt`、`tests/CMakeLists.txt` 一致；新增依赖先更新该文件 |
| **官方仓库生态、角色边界与文档归属** | [`REPOSITORY-MAP.md`](./REPOSITORY-MAP.md) | 其它文档只链接，不重复维护仓库总表 |
| **五层编译流水线** goldens（Lexer/IR/…） | `docs/assets/workflow/FIVE-LAYER-PIPELINE.md` | 与 `TEST-CATALOG` §9 交叉链接 |
| 开发文档目录与维护准则（含本节） | `DOCUMENTATION-POLICY.md` | 链接 |
| Agent 实现规程、禁止项、流水线 | `AGENT-SPEC.md` | 链接 |
| Golden Cross **守则内嵌的宪法示例代码** | `AGENT-SPEC.md` §12.3 | 设计背景链到 `../design/Styio-Resource-Topology.md` §8 |
| Topology v2 **实施步骤、修改点矩阵、风险与记录规范** | `../plans/Resource-Topology-v2-Implementation-Plan.md` | `../design/Styio-Resource-Topology.md` §9 仅状态表 + 链到本计划 |
| **`[|n|]` 环缓 CodeGen** 从 bootstrap 迁移的破坏面 / 测试 / 回滚 | `../plans/BoundedRing-Codegen-Adjustment.md` | 会话细节见 `history/2026-03-29.md` |
| **Checkpoint 微里程碑执行规则**（可中断/可恢复） | `../assets/workflow/CHECKPOINT-WORKFLOW.md` | 在 `history/YYYY-MM-DD.md` 写恢复指引，不在其它文档重复流程细节 |
| **仓库清理、提交、push 与历史重写标准** | `../assets/workflow/REPO-HYGIENE-COMMIT-STANDARD.md` | 其它文档只保留入口规则与链接 |
| **文档元数据、生成索引与审计流程** | `../assets/workflow/DOCS-MAINTENANCE-WORKFLOW.md` | 其它文档只保留入口规则与链接 |
| **架构决策记录（ADR）** | `docs/adr/` | 决策摘要可在 history 引用，正文仅维护于 ADR 文件 |

### 0.5 文档状态与 superseded 规则

1. `docs/plans/*.md` 是**设计/实施计划**，不是语言或验收层面的 SSOT。
2. `docs/milestones/<YYYY-MM-DD>/` 下的文档是该批次的**冻结规格**；若后续实现保留兼容层，文档必须明确区分：
   - **canonical**：冻结示例与推荐写法；
   - **accepted compatibility shorthand**：实现保留、测试覆盖、但不作为首选教学写法的兼容写法。
3. 同一功能若存在较早草案和较晚冻结批次，较早文档必须在文首显式写：
   - `Status: Superseded draft`
   - 指向新的冻结文档路径。
4. 当实现接受的兼容语法多于冻结示例时，SSOT 必须说明“为什么该语法仍有效”，并至少有一条自动化测试冻结该兼容行为。

### 0.6 文档目录职责

| 路径 | 存放内容 |
|------|----------|
| `docs/design/` | 语言设计、EBNF、符号表、资源/标准库等设计级 SSOT |
| `docs/specs/` | agent / contributor 规范、文档策略、依赖规范 |
| `docs/review/` | review 发现、设计冲突、待定决议 |
| `docs/plans/` | 设计草案、实施计划、迁移方案；非冻结规格 |
| `docs/assets/workflow/` | 可复用工作流、测试框架、checkpoint / hygiene 标准 |
| `docs/assets/templates/` | 可复用模板 |
| `docs/history/` | 按日开发历史与恢复记录 |
| `docs/milestones/` | 按日期冻结的里程碑规格 |
| `docs/adr/` | 架构决策记录 |

### 0.7 文件命名约定

1. `docs/design/`：设计级 SSOT 使用稳定、可搜索的主题名；当前约定为 `Styio-*.md`。
2. `docs/specs/`：规范文件使用稳定、可搜索的全大写短横线命名。
3. `docs/plans/`：计划文件必须使用描述性名称，优先 `<Topic>-Plan.md`、`<Topic>-Implementation-Plan.md`、`<Topic>-Adjustment.md`；禁止再新增 `idea.md`、`notes.md`、`misc.md` 这类泛名文件。
4. `docs/assets/workflow/` 与 `docs/assets/templates/`：可复用资产采用稳定、可搜索的全大写短横线命名。
5. `docs/history/`：严格使用 `YYYY-MM-DD.md`。
6. `docs/adr/`：严格使用 `ADR-XXXX-<slug>.md`。
7. `docs/milestones/`：目录使用 `YYYY-MM-DD/`，文件使用 `00-Milestone-Index.md` 与 `M<id>-<Topic>.md`。

### 0.8 Directory Entry Rules

1. Every collection directory under `docs/` must provide both `README.md` and `INDEX.md`.
2. `README.md` owns **scope, naming, and maintenance rules** only.
3. `INDEX.md` owns the **generated inventory** for that directory.
4. `README.md` must point readers to `INDEX.md`; it should not duplicate a full file inventory.
5. Leaf bundles such as dated milestone batches may keep their existing entry file (for example `00-Milestone-Index.md`) and do not need an extra nested `INDEX.md`.
6. Adding a new top-level collection directory requires updating `docs/README.md`, this policy, and the docs-index generator configuration.

### 0.9 Generated Index Rules

1. Collection-directory `INDEX.md` files are generated by `python3 scripts/docs-index.py --write`.
2. Generated indexes must not be hand-maintained.
3. Structural validation runs through `python3 scripts/docs-audit.py`, `ctest --test-dir build -L docs`, and the `checkpoint-health` workflow.
4. A docs-tree change is not complete until the generated indexes and docs audit both pass.

---

## 1. 目标

- **历史（history）**：所有开发经验、排错记录、进展摘要按 **自然日** 写入 `docs/history/`，一天一篇或同日增量追加，禁止只写在聊天或未入库笔记里。
- **里程碑（milestones）**：里程碑规格、验收说明按 **日期目录** 归档在 `docs/milestones/<YYYY-MM-DD>/`（例如 `docs/milestones/2026-03-29/`）。索引文件为该目录下的 `00-Milestone-Index.md`。
- **测试说明（workflow assets）**：面向读者的测试说明按 **功能域** 维护在 `docs/assets/workflow/TEST-CATALOG.md`，与 CMake 中的 `add_test` 一一可追溯；**必须**给出可复制的自动化命令（CTest 标签或正则）。
- **可机读元数据**：凡描述「某测试在测什么」的文档，须在文首或表格中写明 **Last updated**、**输入**、**期望输出/比对物**（golden 路径或约定临时文件），以便脚本与人工对照。

---

## 2. 历史文档 `docs/history/`

| 规则 | 说明 |
|------|------|
| 命名 | `YYYY-MM-DD.md`，与日历日一致。 |
| 内容 | 当日实现决策、踩坑、与里程碑/PR 的对应关系；可链接到具体提交或文件路径。 |
| 索引 | `docs/history/README.md` 列出文件与一句话摘要（可手改或由 CI 校验存在性）。 |

文首建议模板：

```markdown
# 开发记录 — YYYY-MM-DD

**Last updated:** YYYY-MM-DD

## 摘要
…
```

---

## 3. 里程碑文档 `docs/milestones/<YYYY-MM-DD>/`

| 规则 | 说明 |
|------|------|
| 目录 | 一次「里程碑冻结」或重大规划使用一个日期文件夹；其下 `M1-*.md` … `M7-*.md` 与 `00-Milestone-Index.md`。 |
| 文首 | 写明 **Date** / **Last updated**（与目录日期可不同，但需真实）。 |
| 与测试关系 | 验收用例名称应与 `tests/milestones/m*/t*.styio` 及 `docs/assets/workflow/TEST-CATALOG.md` 对齐；若规格中有而仓库尚无 `.styio`，须在规格与目录中标注 **gap**。 |

索引：`docs/milestones/README.md` 指向各日期子目录。

---

## 4. 测试目录 `docs/assets/workflow/TEST-CATALOG.md`

| 规则 | 说明 |
|------|------|
| 划分维度 | **按语言功能域**（与 M1–M7 主题对齐），而非仅按内部文件名。 |
| 每条目 | 至少包含：**CTest 名**、**输入**（`.styio` 路径）、**输出/Oracle**（`expected/*.out` 或文档约定的临时文件路径）、**自动化**（`ctest -R '…'` 或 `ctest -L …`）。 |
| 与构建一致 | 新增 `.styio` 验收测试时，必须同时更新 `tests/CMakeLists.txt`（或项目约定的单一注册处）与 `../assets/workflow/TEST-CATALOG.md`。 |

单条示例（字段名固定，便于将来脚本解析）：

| CTest | Input | Oracle | Automation |
|-------|-------|--------|------------|
| `m1_t01_int_arith` | `tests/milestones/m1/t01_int_arith.styio` | `tests/milestones/m1/expected/t01_int_arith.out` | `ctest --test-dir build -R '^m1_t01_int_arith$'` |

---

## 5. 与 `AGENT-SPEC.md` 的关系

语言与编译器实现规范仍以 [`AGENT-SPEC.md`](./AGENT-SPEC.md) 为准；**文档存放位置、历史/里程碑/测试目录约定及 §0 维护准则** 以本文件为准。二者冲突时，先更新本策略与索引，再改 `AGENT-SPEC` 中的引用。

---

## 6. Automation Gates

CI 或本地可逐步引入：

1. `ctest -L milestone` 全绿（或允许已知失败列表，但须在 `TEST-CATALOG` 标注）。
2. `python3 scripts/docs-index.py --check` 必须通过，确保 collection-directory `INDEX.md` 未过期。
3. `python3 scripts/docs-audit.py` 必须通过，确保 `Purpose` / `Last updated` / 命名 / 链接 / 目录入口都符合规则。
4. `../assets/workflow/TEST-CATALOG.md` 中列出的每个 `tests/milestones/...` 路径在仓库中存在。

当前仓库的 **权威自动化入口** 为：**CMake 注册的 CTest + `styio --file`**（见 `tests/CMakeLists.txt`）。
