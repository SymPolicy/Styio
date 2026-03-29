# Styio 开发文档策略

**文档作用：** 约定开发类 Markdown 的存放位置、历史与测试索引格式、**单一事实来源（SSOT）** 的引用方式，以及下文 **§0** 的维护准则；**不**定义语言语义（语义见 `Styio-Language-Design.md` 等）。

**Last updated:** 2026-03-28（增补 §0 维护准则与 SSOT 表）  
**Automation (verify doc links + test registration):** 从仓库根目录配置并运行里程碑测试：

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build -L milestone
```

（GoogleTest 目标 `styio_test` 需单独构建；若本机 LLVM 与 libstdc++ 头文件冲突导致 gtest 编译失败，仍以 `styio` 可执行文件与 `ctest -L milestone` 为准。）

---

## 0. 文档维护准则（最小改动与单一事实来源）

### 0.1 文首「文档作用」

除本文件或 `AGENT-SPEC.md` 中已用等效语句（如 **Authority** / **Purpose**）说明职责的段落外，每个 `docs/**/*.md` 应在标题后尽快用 **一行 `文档作用：`**（或紧接的英文等价句）说明：读者何时应读本文、本文**不**承担什么职责，以免与其它 SSOT 重叠。

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
| 语言语义与章节结构 | `Styio-Language-Design.md` | 链到章节，避免大段复述 |
| 词法与文法 EBNF | `Styio-EBNF.md` | 链接 |
| 符号 ↔ lexer token 名 | `Styio-Symbol-Reference.md` | 链接 |
| `@` 拓扑目标语法、Golden Cross **设计级**叙述与示例形态 | `Styio-Resource-Topology.md`（含 §8） | 保留链接或一句摘要 |
| 设计 / 实现冲突与待定决议 | `Logic-Conflicts.md` | 链接 |
| M1–M7 路线图、依赖链、规格文件表 | `docs/milestones/<日期>/00-Milestone-Index.md` | **勿**在 history 等处平行维护同一张总表 |
| 集成测试路径、`ctest` 命令 | `docs/tests/TEST-CATALOG.md` | 链接 |
| 开发文档目录与维护准则（含本节） | `DOCUMENTATION-POLICY.md` | 链接 |
| Agent 实现规程、禁止项、流水线 | `AGENT-SPEC.md` | 链接 |
| Golden Cross **守则内嵌的宪法示例代码** | `AGENT-SPEC.md` §12.3 | 设计背景链到 `Styio-Resource-Topology.md` §8 |
| Topology v2 **实施步骤、修改点矩阵、风险与记录规范** | `Resource-Topology-v2-Implementation-Plan.md` | `Styio-Resource-Topology.md` §9 仅状态表 + 链到本计划 |
| **`[|n|]` 环缓 CodeGen** 从 bootstrap 迁移的破坏面 / 测试 / 回滚 | `BoundedRing-Codegen-Adjustment.md` | 会话细节见 `history/2026-03-29.md` |

---

## 1. 目标

- **历史（history）**：所有开发经验、排错记录、进展摘要按 **自然日** 写入 `docs/history/`，一天一篇或同日增量追加，禁止只写在聊天或未入库笔记里。
- **里程碑（milestones）**：里程碑规格、验收说明按 **日期目录** 归档在 `docs/milestones/<YYYY-MM-DD>/`（例如 `docs/milestones/2026-03-29/`）。索引文件为该目录下的 `00-Milestone-Index.md`。
- **测试说明（tests）**：面向读者的测试说明按 **功能域** 维护在 `docs/tests/TEST-CATALOG.md`，与 CMake 中的 `add_test` 一一可追溯；**必须**给出可复制的自动化命令（CTest 标签或正则）。
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
| 与测试关系 | 验收用例名称应与 `tests/milestones/m*/t*.styio` 及 `docs/tests/TEST-CATALOG.md` 对齐；若规格中有而仓库尚无 `.styio`，须在规格与目录中标注 **gap**。 |

索引：`docs/milestones/README.md` 指向各日期子目录。

---

## 4. 测试目录 `docs/tests/TEST-CATALOG.md`

| 规则 | 说明 |
|------|------|
| 划分维度 | **按语言功能域**（与 M1–M7 主题对齐），而非仅按内部文件名。 |
| 每条目 | 至少包含：**CTest 名**、**输入**（`.styio` 路径）、**输出/Oracle**（`expected/*.out` 或文档约定的临时文件路径）、**自动化**（`ctest -R '…'` 或 `ctest -L …`）。 |
| 与构建一致 | 新增 `.styio` 验收测试时，必须同时更新 `tests/CMakeLists.txt`（或项目约定的单一注册处）与 `TEST-CATALOG.md`。 |

单条示例（字段名固定，便于将来脚本解析）：

| CTest | Input | Oracle | Automation |
|-------|-------|--------|------------|
| `m1_t01_int_arith` | `tests/milestones/m1/t01_int_arith.styio` | `tests/milestones/m1/expected/t01_int_arith.out` | `ctest --test-dir build -R '^m1_t01_int_arith$'` |

---

## 5. 与 `AGENT-SPEC.md` 的关系

语言与编译器实现规范仍以 `docs/AGENT-SPEC.md` 为准；**文档存放位置、历史/里程碑/测试目录约定及 §0 维护准则** 以本文件为准。二者冲突时，先更新本策略与索引，再改 `AGENT-SPEC` 中的引用。

---

## 6. 自动化检查（建议）

CI 或本地可逐步引入：

1. `ctest -L milestone` 全绿（或允许已知失败列表，但须在 `TEST-CATALOG` 标注）。
2. `docs/history/*.md`、`docs/milestones/*/00-Milestone-Index.md` 存在且含 `Last updated` 或 `Date` 行（可用简单 grep/lint）。
3. `TEST-CATALOG.md` 中列出的每个 `tests/milestones/...` 路径在仓库中存在。

当前仓库的 **权威自动化入口** 为：**CMake 注册的 CTest + `styio --file`**（见 `tests/CMakeLists.txt`）。
