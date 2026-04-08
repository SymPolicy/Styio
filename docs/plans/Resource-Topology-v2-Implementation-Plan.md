# Resource Topology v2 — 新语法实施计划

**文档作用：** 将 [`../design/Styio-Resource-Topology.md`](../design/Styio-Resource-Topology.md) 中的 **目标语法** 落实为可执行的 **分阶段重构清单**（改哪些模块、测什么、记什么日志）；**不**重复拓扑设计全文（以 Topology 为 SSOT），**不**替代 [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md) 中的冲突登记（实施中解决一项应回写该文档或注明已关闭）。

**Last updated:** 2026-03-28（环缓 CodeGen 迁移见 [`BoundedRing-Codegen-Adjustment.md`](./BoundedRing-Codegen-Adjustment.md)）  
**Normative design:** [`../design/Styio-Resource-Topology.md`](../design/Styio-Resource-Topology.md)  
**Grammar sketch:** [`../design/Styio-EBNF.md`](../design/Styio-EBNF.md) Appendix B  
**Open conflicts:** [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md)

---

## 1. 范围与成功标准

### 1.1 交付物（语法 / 语义）

| # | 能力 | 设计出处 | 当前仓库（截至本计划） |
|---|------|----------|------------------------|
| T1 | 词法 **`[|`**、**`|]`** 与类型 **`[|n|]`** | Topology §3, EBNF B.2 | 仅有 `[` `]` 单字 token，无配对界符 |
| T2 | 顶层 **`ResourceDecl`**：`@id : Type { , @id : Type } [ := DriverBlock ]` | Topology §4, EBNF B.1 | 顶层无专用资源声明；`@`+`[` 走 `parse_state_decl_after_at`（块内 M6） |
| T3 | **`DriverBlock`** 内保留现有 `StreamTopology`（`>>` / `#()` 等） | Topology §4 | 已有管道解析，需接到新顶层节点下 |
| T4 | 语句 **`expr -> $name`**（写入资源影子） | Topology §5, EBNF B.3 | `->` 可能已用于 M5 redirect；需与 **影子写入** 合一或分语境 |
| T5 | 语义：**禁止** 资源影子用 **`$x = expr`**，报错并引导 `->` | Topology §5 | 当前赋值路径未区分 local vs shadow |
| T6 | 语义：**顶层以外** 禁止无迁移路径的 `@name : …`（Topology §4 Forbidden） | Topology §4 | 未实现 |
| T7 | 与 M6 **`@[n](…)`** 的 **共存或迁移** 策略（向后兼容窗口） | Topology §9, Logic-Conflicts §1.3 | M6 为现行 canonical |

### 1.2 成功标准（工程）

1. **回归：** 现有 `tests/milestones/m1`–`m7` 在「兼容模式」下仍可通过（除非明确宣布 breaking 批次并批量更新 golden）。  
2. **新测：** 至少一组 `tests/milestones/m8/`（或 `topology_v2/`）覆盖 T1–T5 的最小用例；[`docs/assets/workflow/TEST-CATALOG.md`](../assets/workflow/TEST-CATALOG.md) 与 `tests/CMakeLists.txt` 同步更新。  
3. **文档：** 每阶段结束在 [`docs/history/YYYY-MM-DD.md`](./history/) 记 **会话日志**（见 §7）；关闭的冲突回写 `../review/Logic-Conflicts.md`。

---

## 2. 分阶段路线图（建议）

> 阶段边界可按人力调整；**顺序**建议保持：词法 → 顶层 AST → 块内语句 → 语义 → IR/代码生成 → 测试与迁移。

| 阶段 | 名称 | 主要产出 | 退出准则 |
|------|------|----------|----------|
| P0 | 基线与开关 | 文档、feature flag（可选）、基线 `ctest -L milestone` 记录 | 计划评审通过；history 中有基线条目 |
| P1 | 词法 `[|n|]` | 新 token、`Tokenizer.cpp` 单测或最小 `.styio` | `[|3|]` 可 tokenize，不与 `[` 子表达式混淆 |
| P2 | 顶层 `ResourceDecl` 解析 | 新 AST 或扩展现有 Program 子节点 | 可 parse Topology §4 多资源示例（不要求跑通 JIT） |
| P3 | `expr -> $id` | 新语句 AST + parser | 可解析 Golden Cross 草图（设计级）中的 `->` 行 |
| P4 | 语义层 | 符号表：资源名 / 影子 vs local；`$x =` 拒绝规则 | 单元测试或诊断用例覆盖 |
| P5 | IR / 分配 / CodeGen | `ToStyioIR`、ledger 与 M6 路径对齐或分支 | 最小程序端到端执行 |
| P6 | 迁移与清理 | 文档、可选 deprecate M6 表面语法、更新 Topology §9 状态表 | 目标语法在 §9 中标为 Implemented |

---

## 3. 修改点清单（按子系统）

### 3.1 词法 `src/StyioToken/`

| 文件 / 位置 | 修改内容 |
|-------------|----------|
| `Token.hpp` | 新增 `TOK_LBOXBRAC_PIPE` / `TOK_PIPE_RBOXBRAC`（或命名等价）表示 **`[|`**、**`|]`**；更新 `check_pattern` / 最大吞噬顺序，确保 **`[|`** 优先于 `[` + `|` |
| `Tokenizer.cpp` | 在 `[` 分支 lookahead：若下一字符为 `\|`，发出 **`[|`**；对称处理 `\|` + `]` |
| `Token.cpp`（若有） | 新 token 的 debug / toString |
| **Visitor / 枚举同步** | 凡 `switch (TokenKind)` / `StyioNodeType` 全覆盖处：`ToString.cpp`、`Parser` 错误消息、任何 exhaustive enum 处理 |

**依赖：** 与 `[` 用于 stream/infinite、`[` `<<` 历史探针等 **无字符级冲突**（`[` 仍为 `[`，`[|` 为双字符起点）。若 `|[` 出现在用户代码中需明确是否非法。

### 3.2 语法 `src/StyioParser/Parser.cpp`（及可能的 `Parser.hpp`）

| 任务 | 说明 |
|------|------|
| **程序入口** | 定位当前 **顶层语句列表** 的解析循环（如 `parse_stmt_or_expr` 批量调用处）；在 **`@` + `Identifier` + `:`** 形态下进入 **`parse_resource_decl_v2`**，而非直接进入 `parse_state_decl_after_at` |
| **`parse_resource_decl_v2`** | 解析 `@ name : type` 重复 `, @ name : type`；可选 `:= { ... }`；`type` 含 **`[|n|]`** 与标量 |
| **`parse_type`**（或等价） | 扩展以消费 **`[|n|]`**（整数容量） |
| **`parse_stmt_or_expr` / 语句** | 识别 **`expression TOK_ARROW_RIGHT $ Identifier`**（需确认 `->` 现有 token 是组合还是两 token `>`） |
| **`parse_state_decl_after_at`** | 保留 M6 路径；与顶层 `@id :` 的 **FIRST 集分离**（避免把 `@ma5 : [|2|]` 误切入 `@[`） |

**注意：** 查阅现有 `->` 是否已映射为 `TOK_ARROW_RIGHT` 或与 `>>` 冲突；Logic-Conflicts §1.2 `>>` 与 continue 已有 disambiguation，新语句不得破坏。

### 3.3 AST `src/StyioAST/`

| 方向 | 说明 |
|------|------|
| **新节点 `ResourceDeclAST`（推荐）** | 字段：`vector<NamedSlot>{ name, type }`、`optional<DriverBlockAST*>`；`DriverBlock` 可复用 `BlockAST` 或包装现有 stream AST |
| **或** 扩展 `Program` / 顶层列表 | 若当前无统一 `ProgramAST`，在 `main.cpp` 收集处增加新 stmt 类型 |
| **新节点 `StateWriteAST` 或 `SinkAssignAST`** | `expr`、`target $name`；与 `AssignAST` 区分 |
| **`StateDeclAST`** | 短期保留；长期可标记 deprecated 或与 `ResourceDecl` 共享后端 lowering |

**必做机械工作：** `AST.hpp` 新类 → `ASTDecl.hpp` visitor 声明 → **所有** visitor 实现文件（`TypeInfer`、`ToStyioIR`、`CodeGen*`、`ToString`、可能的 `ASTAnalyzer`）注册，避免 AGENT-SPEC 禁止的「半注册」。

### 3.4 语义 `src/StyioAnalyzer/`

| 文件 | 修改内容 |
|------|----------|
| `TypeInfer.cpp` | 新节点 `typeInfer`；`[|n|]` 的元素类型 / 与 `$name` 读取类型一致化 |
| **符号与环境** | 顶层 **`@ma5`** 与块内 **`$ma5`** 绑定；**local `x =`** 不与 shadow 混淆 |
| **拒绝规则** | 对 **resource shadow** 的 **`$x =`** 报错（需定义「何谓 shadow」：由 `ResourceDecl` 引入的名字集合） |
| `ToStyioIR.cpp` | `lower_resource_decl`、`lower_state_write`；与现有 `StateDeclAST`、`SGStateSlotDesc`、`classify_state_slot` **对齐或分支** |
| 可能 `ASTAnalyzer.hpp` | 根级只允许 `ResourceDecl` 的校验 |

### 3.5 代码生成 `src/StyioCodeGen/`（路径以仓库为准）

| 内容 |
|------|
| 新 IR 节点 → LLVM：影子写入是否复用现有 store 到 ledger 槽的路径 |
| **`main` 返回值**、phi、与 M6 frame lock 交互（见 history 2026-03-29） |

### 3.6 CLI / 入口 `src/main.cpp`

| 内容 |
|------|
| 若增加 `--syntax=m6|v2` 或类似 flag，在此解析并传入 `StyioContext` |
| `--styio-ast` 输出包含新节点 |

### 3.7 测试与构建

| 位置 | 内容 |
|------|------|
| `tests/CMakeLists.txt` | 新标签 `m8` 或 `topology_v2` |
| `tests/milestones/...` | 新 `.styio` + `expected/*.out` |
| `docs/assets/workflow/TEST-CATALOG.md` | 新功能域小节 |
| `extend_tests.py` | 可选：生成脚手架 |

### 3.8 设计文档（随实现闭合）

| 文档 | 动作 |
|------|------|
| `../design/Styio-Resource-Topology.md` §9 | 逐项更新 **Implementation status** |
| `../design/Styio-EBNF.md` Appendix B | 将「target-only」改为「已实现」子集时同步 |
| `../design/Styio-Language-Design.md` | 与 Topology 冲突段落改为「见 Topology + 实现版本」 |
| `../review/Logic-Conflicts.md` | 已解决的 `@` / `->` / `<<` 条目关闭或指向新 disambiguation 节 |

---

## 4. 已知困难与风险（预登记）

### 4.1 词法与文法

| ID | 风险 | 缓解 |
|----|------|------|
| L1 | **`[|` 与 `[` + 模式** 在复杂嵌套中误切 | 严格单元测试；禁止在 `[|` 内嵌未转义 `|]` 闭包外的裸 `\|`（若语法如此规定） |
| L2 | **`|]`** 与管道 `\|`、逻辑或混淆 | Tokenizer 仅在 **`[|` 开启的模式**下识别 `\|]` |
| L3 | **`->` 双字符** 与 `>` 比较符、`-` 负号 | 沿用现有 maximal munch；新语句形态用 lookahead 确认 `$` |

### 4.2 与现行 M6/M7 共存

| ID | 风险 | 缓解 |
|----|------|------|
| C1 | 同一标识在 **`@[5](x=…)`** 与 **`@x : [|5|]`** 下 IR 布局不同 | 明确 **迁移指南** 或 **双模式** lowering 映射到同一 ledger 抽象 |
| C2 | 测试 golden **大规模变更** | 分 PR：先加 v2 测试不删 M6；再可选迁移 golden |
| C3 | **`<<` 五义性**（Logic-Conflicts §1.1）在资源块内加剧 | 实施前写出与 `../design/Styio-EBNF.md` 一致的 **位置表**；新代码禁止再增第六种无文档含义 |

### 4.3 语义与类型

| ID | 风险 | 缓解 |
|----|------|------|
| S1 | **`$name` 有时 local capture、有时全局影子** | 以 **声明点** 区分：仅 `ResourceDecl` 引入的为 shadow；文档化 |
| S2 | **`[|n|]` 元素类型** 未写死（标量 vs 结构） | 第一期只支持 Topology 示例中的 **标量槽** |
| S3 | **intrinsic 隐式缓冲** vs **用户可见 `@ma : [|2|]`**（Topology §7） | 保持 **双槽模型**；在 ToStyioIR 注释或文档中标注 fingerprint |

### 4.4 IR / CodeGen

| ID | 风险 | 缓解 |
|----|------|------|
| G1 | **LLVM verify**（`main` 返回类型、phi）在新区块路径复发 | 每阶段跑 `styio --all`；对照 history §8 共性 |
| G2 | **Frame lock / 多流** 与顶层 driver 单线程假设 | 参考 M7 与 `M7-MultiStream.md`，明确 driver 块是否单线程 |

### 4.5 工程

| ID | 风险 | 缓解 |
|----|------|------|
| E1 | **Visitor 遗漏** 导致链接期或模板爆炸 | 使用编译器警告 + AGENT-SPEC checklist |
| E2 | **GoogleTest 与 LLVM 头冲突**（部分环境） | 以 **`ctest -L milestone`** 为主验收；gtest 可选 |

---

## 5. 开发过程记录规范（强制）

与 [`../specs/DOCUMENTATION-POLICY.md`](../specs/DOCUMENTATION-POLICY.md) §0 一致，并 **额外** 要求本专题：

1. **每个工作日或每个合并批次** 在 `docs/history/YYYY-MM-DD.md` 增加一小节 **`## Topology v2 / M8`**，至少包含：  
   - 本日完成的阶段（P1–P6）  
   - 改动的 **文件路径列表**  
   - **未解决问题** / 明日计划  
   - 相关 **PR / commit**（若有）  
2. **关闭 Logic-Conflicts 条目时**：在同一 history 日条目中写 **一行引用**（哪一节、如何解析）。  
3. **本计划** 仅在 **阶段目标或风险矩阵** 变化时更新 **Last updated** 与对应表格；细节流水不写回本文件（避免与 history 重复）。

---

## 6. 建议的首个可交付里程碑（M8 最小切片）

**目标：** 仅解析 + AST 打印 + 语义报错骨架，**不要求** 完整 JIT。

1. P1：`[|2|]` tokenize + parse 为类型节点。  
2. P2：单资源 ` @a : [|2|] := { … }` 无体内复杂逻辑，体内为空块或单 `>_("ok")`。  
3. P3：一行 `1 -> $a` 解析；若尚未有资源声明则语义报错 **清晰**。  

**验收：** `styio --file tests/.../t01_minimal.styio --styio-ast` 结构正确；`ctest -R` 绑定 golden 可选后置。

---

## 7. 开项（需在实施前拍板）

| 议题 | 选项 | 影响 |
|------|------|------|
| M6 语法废弃时间表 | 永久双轨 / 废弃期 + warning / 立即 breaking | 测试与文档工作量 |
| `<<` 作为 return 是否迁移 | 保留 `<<` / 迁至 `<|` | Lexer + 全测试 |
| `->` 在 M5 文件重定向与 v2 影子写入 | 同一 AST 节点 / 分两节点 | Parser 与 TypeInfer |

---

## 8. 参考：Parser 现状锚点（便于检索）

- `parse_state_decl_after_at`：`Parser.cpp`（M6 `@`+`[` 路径）  
- `TOK_AT` 在 `parse_stmt_or_expr` 的分支：`Parser.cpp`（需与顶层 `@Identifier` 区分）  
- `StateDeclAST`：`AST.hpp`（字段：window、acc、export、update）

**本文件修订历史：** 见 git log；叙事性过程见 `docs/history/`。
