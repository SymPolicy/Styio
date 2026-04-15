# IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan-CN

**Purpose:** 这是 [`IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](./IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md) 的中文镜像版本，用于定义 Styio IDE 子系统从当前 MVP 形态推进到成熟 IDE 级补全与语义能力的完整路线图。该路线图明确要求至少对标 3 套成熟语言工具链，而不是只参考 Rust 一条线；冻结验收目标见 [`../milestones/2026-04-15/`](../milestones/2026-04-15/)，IDE 使用方式见 [`../for-ide/README.md`](../for-ide/README.md)。

**Last updated:** 2026-04-15

**Date:** 2026-04-15  
**Status:** 活跃实施计划。冻结验收文档位于 [`../milestones/2026-04-15/`](../milestones/2026-04-15/)。  
**Depends on:** 现有 IDE/LSP 基础、Tree-sitter backend 集成、Nightly `ParseMode::Recovery`，以及当前 `styio_ide_core` / `styio_lspd` 构建目标。  
**Related docs:** [`./IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](./IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md), [`../for-ide/BUILD.md`](../for-ide/BUILD.md), [`../for-ide/TREE-SITTER.md`](../for-ide/TREE-SITTER.md), [`../for-ide/CXX-API.md`](../for-ide/CXX-API.md), [`../specs/DOCUMENTATION-POLICY.md`](../specs/DOCUMENTATION-POLICY.md)

---

## 1. 目标

当前目标不只是“让 IDE 内核支持增量更新”。真正的目标，是补齐 Styio IDE 子系统与成熟语言 IDE 工具链之间最大的结构差距：

1. 真正的多段增量语法更新
2. 明确成 query 形状的语义缓存
3. 具有稳定身份的 HIR
4. 基于真实作用域图的词法+语义名字解析
5. item 级类型推断复用
6. 由语法上下文、作用域、接收者类型和排序策略共同驱动的补全
7. 工作区级符号/引用索引
8. 后台运行时控制、取消机制和性能门槛

这条路线图明确至少对标 3 套外部工具链族：

1. C++：`clang` / `clangd`
2. Python：CPython PEG parser 加 `parso` / `pyright` 风格编辑态工具
3. Rust：`rustc` / `rust-analyzer`

目标不是照搬其中任何一套，而是吸收三者最成熟的做法：

- C++ 提供“复用编译器真相”、后台索引和工作区导航分层
- Python 提供容错编辑态解析，以及在残缺代码下继续工作的 best-effort 语义服务
- Rust 提供 query 驱动语义、稳定中间层表示和高质量补全架构

本计划冻结 `M11-M19` 的实现顺序。

---

## 2. 当前缺口

### 2.1 基础设施缺口

当前行为：

1. [`src/StyioLSP/Server.cpp`](../../src/StyioLSP/Server.cpp) 在重要场景下仍会把 `didChange` 退化成简化的整文档路径。
2. [`src/StyioIDE/VFS.cpp`](../../src/StyioIDE/VFS.cpp) 还没有向所有消费者暴露一等公民的多段 edit delta 边界。
3. [`src/StyioIDE/SemDB.cpp`](../../src/StyioIDE/SemDB.cpp) 的公共契约仍围绕一个粗粒度的 `IdeSnapshot`。

后果：

- 语法和语义失效范围仍然过大
- cache hit 行为难以观测和优化
- 未来 item 级复用没有稳定的依赖图可挂接

### 2.2 语义模型缺口

当前行为：

1. [`src/StyioIDE/HIR.cpp`](../../src/StyioIDE/HIR.cpp) 仍然偏实用，而不是完全规范化。
2. definition/reference 还没有建立在完整作用域图之上。
3. 类型信息的缓存粒度还不足以支撑高质量补全。

后果：

- 未受影响的 item 更难在编辑后保留身份
- 局部遮蔽和 import 解析规则更难精确实现
- 基于接收者类型和成员能力的补全质量仍落后于成熟的 C++、Python、Rust IDE 工具链基线

### 2.3 运行时和质量缺口

当前行为：

1. 后台任务、取消和 debounce 仍然比较薄弱
2. workspace index 分层还不够完整
3. fuzz / 性能 / 漂移门槛还不完整

后果：

- 过期请求会浪费工作
- 跨文件导航和 workspace symbol 难以平稳扩展
- 性能回退可能在用户感知前就潜入系统

---

## 3. 架构不变量

这些决定在整个路线图周期内固定不变：

| 范围 | 决策 |
|------|----------|
| 仓库布局 | 语言真相和 IDE 真相继续放在主仓库 `styio` 内 |
| parser 分层 | Tree-sitter 风格 syntax parser 负责编辑态结构；Nightly parser 继续作为语义/编译真相 |
| snapshot 身份 | 所有语义/缓存边界都以 `FileId` 和 `SnapshotId` 为键 |
| HIR 边界 | IDE 功能消费 HIR、scope graph 和 query 结果，而不是直接遍历 AST 类层级 |
| completion | parser 只提供上下文和 expected categories；语义层提供具体候选和排序 |
| indexing | 打开文件状态、后台索引和持久化索引保持分层 |
| runtime | 前台延迟优先于后台索引和维护任务 |

在整个路线图周期内，还固定保持以下对标基线：

| 工具链族 | 参考工具 | Styio 需要吸收的经验 |
|------|----------|----------|
| C++ | `clang`, `clangd` | 尽量复用编译器真相，并明确区分 open-file index 和 background index 职责 |
| Python | CPython PEG parser, `parso`, `pyright` | 保持编辑态解析容错，并让语义服务在不完整代码下继续 best-effort 工作 |
| Rust | `rustc`, `rust-analyzer` | 使用 query 形状的语义层、稳定中间表示，以及建立在语义上下文上的补全而不是 token 猜测 |

---

## 4. 实现前冻结决策清单

路线图已经足够清晰，可以进入实现，但下面这些决策需要在进入对应里程碑前冻结。如果继续保持隐式，会造成明显返工。

| 决策领域 | 必须冻结什么 | 推荐基线 | 最晚冻结时点 | 为什么重要 |
|------|----------|----------|----------|----------|
| LSP 文档同步契约 | `textDocument/didChange` 是否默认按有序增量 edit 处理，以及 full-sync client 如何回退 | 增量同步是主路径；整文档替换保留为兼容 fallback | 在 `M11` 前 | 这会直接决定 `IdeService::did_change`、VFS 更新接口和所有 edit 路径测试的公共形状 |
| 内部文本坐标模型 | LSP `Position` 如何映射到内部 offset | UTF-16 `line/character` 只保留在 LSP 边界；进入内部后立即统一转换为 UTF-8/byte offset | 在 `M11` 前 | 如果这里不明确，`TextEdit`、`TSInputEdit`、diagnostic 和导航 range 在非 ASCII 文本上都会漂移或出错 |
| delta 规范化与合并 | 多个 edit 如何规范化、排序以及按需合并 | 保留 LSP 原始顺序；先基于当前工作文本规范化；只有在规范化之后才合并重叠或相邻 edit | 在 `M11` 前 | Tree-sitter 增量复用依赖正确的 edit 坐标；错误合并会损坏树或导致过度失效 |
| snapshot 与语义 ID 稳定性 | `SnapshotId`、`ItemId`、`ScopeId`、`SymbolId` 中“稳定”的含义 | `SnapshotId` 每次文档变更都更新；语义 ID 在一次会话内对无关编辑保持稳定，但第一版不要求跨重启稳定 | 在 `M13` 前 | HIR 身份、cache 复用和 index 所属关系都依赖这个契约 |
| module 与 import 模型 | 文件到 module 的映射规则，以及 import path 的解析规则 | 一个源码文件映射到一个主 module；import 解析感知 project；同一作用域下 local 优先于 import，import 优先于 builtin，除非语法另有规定 | 在 `M14` 前 | 没有统一 module/import 模型，名字解析、跨文件 definition 和 workspace index 会前后矛盾 |
| builtin 与 capability 元数据来源 | builtin 符号、方法和 capability 标志来自哪里 | 为 builtin 和 capability-bearing types 保持一层 SSOT 元数据；IDE 只能通过语义 query 读取，不能各处散落 ad hoc 表 | 在 `M14` 前 | resolver、hover、member completion 和 diagnostics 都必须共享同一份 builtin 真相 |
| IDE v1 的类型系统范围 | 这一轮路线图里 IDE 语义到底要支持哪些类型特性 | 第一版必须支持函数签名、receiver/member typing、参数 expected type 和 capability-based filtering；更深的 generic/overload 行为可以保守处理 | 在 `M15` 前 | 类型推断工作很容易膨胀；这个范围决定 M15/M16 是否可控 |
| completion 排序策略 | 不同上下文里的候选如何过滤和排序 | exact prefix 高于 fuzzy；local/param 高于同文件顶层；同文件顶层高于 import；import 高于 builtin；builtin 高于 keyword；keyword 高于 snippet | 在 `M16` 前 | 如果排序策略不冻结，补全质量会变成主观判断，回归测试也会变弱 |
| index 新鲜度模型 | 打开文件、后台索引和持久化索引如何交互 | 未保存的打开文件状态始终覆盖后台和持久化索引；后台索引读取磁盘项目文件；持久化索引只负责冷启动预热 | 在 `M17` 前 | 如果优先级不明确，跨文件 definition/reference 会和打开缓冲区产生冲突 |
| runtime 调度模型 | 前台请求、语义 diagnostics 和后台索引如何调度 | 一个前台请求通道加低优先级后台工作；语义 diagnostics 做 debounce；过期请求由 snapshot/version guard 丢弃 | 在 `M18` 前 | 没有固定 runtime 模型，取消、debounce 和延迟预算就无法一致落地 |
| diagnostic 合并与去重策略 | syntax diagnostics、recovery diagnostics 和 semantic diagnostics 如何组合 | syntax diagnostics 立即发布；semantic diagnostics 延后；recovery 与 semantic 层重复的 range/message 在发布前去重 | 在 `M18` 前 | 否则用户会看到重复或闪烁的 diagnostics，尤其是在 recovery mode 下 |
| 性能测量契约 | 路线图中的延迟预算如何测量和执行 | 在 `M19` 前冻结 benchmark corpus、warm/hot 条件和测量 harness | 在 `M19` 前 | 如果每次测量都用不同输入、缓存状态或机器假设，性能预算就没有意义 |

一旦实现启动后，这些决策如果要改，必须先更新本计划，再回写受影响的 milestone 验收文档。

---

## 5. 决策冻结顺序

上面的决策不需要一次性全部敲定。路线图只要求当前阶段真正会阻塞下一步实现的决策先冻结。

| 优先级 | 何时决定 | 决策项 | 为什么这个时机正确 |
|------|----------|----------|----------|
| `Now` | 恢复 `M11` 实现前 | LSP 文档同步契约；内部文本坐标模型；delta 规范化与合并 | 这三项直接定义 edit pipeline 的形状。不先定下来，`Server`、`IdeService`、`VFS`、`SyntaxParser` 和 Tree-sitter 复用都会立刻返工 |
| `Next gate` | `M13` 开始前，但不阻塞 `M11/M12` | snapshot 与语义 ID 稳定性 | `M11/M12` 只需要文档和 snapshot 级 ID；只有 HIR 身份进入系统时，稳定语义 ID 才成为刚需 |
| `Stage B gate` | `M14` 开始前 | module/import 模型；builtin 与 capability 元数据来源 | resolver 和 scope-graph 必须建立在统一的文件、import 和 builtin 所有权模型上；更早的里程碑不需要冻结所有语义边角 |
| `Stage B gate` | `M15` 开始前 | IDE v1 的类型系统范围 | 如果支持范围保持开放，类型推断工作会快速失控；因此应在 inference-query 工作开始前冻结 |
| `Stage C gate` | `M16` 开始前 | completion 排序策略 | completion 基础设施可以更早存在，但在升级里程碑前若不冻结排序，质量和回归测试都会不稳定 |
| `Stage C gate` | `M17` 开始前 | index 新鲜度模型 | 只有当 open-file、background 和 persistent 三层都同时存在时，索引合并规则才真正重要 |
| `Stage D gate` | `M18` 开始前 | runtime 调度模型；diagnostic 合并与去重策略 | 取消、debounce 和 diagnostic 发布语义应该一起设计，不能等 runtime hardening 已经开始后再临时拼接 |
| `Final gate` | `M19` 开始前 | 性能测量契约 | 性能门槛必须基于冻结的 harness 和 corpus，但它不需要阻塞更早的架构工作 |

推荐的实际决策会顺序：

1. 先冻结三个 `Now` 决策，再恢复 `M11`。
2. 在打开 `M13` 前冻结语义 ID 稳定性。
3. 在打开 `M14` 前冻结 module/import 和 builtin 元数据规则。
4. 在打开 `M15` 前冻结 IDE 类型系统范围。
5. 在打开 `M16` 前冻结 completion 排序策略。
6. 在打开 `M17` 前冻结 index 新鲜度模型。
7. 在打开 `M18` 前冻结 runtime 和 diagnostic 合并行为。
8. 在打开 `M19` 前冻结 benchmark harness 细节。

如果为了方便把后期决策提前，也必须保持同样的依赖方向，不应颠倒。

---

## 6. 交付策略

路线图按阶段推进，前一阶段稳定后才进入下一阶段。

### Stage A — 增量基座

1. `M11` 多段增量 syntax 路径
2. `M12` 文件级/offset 级 query cache

目的：

- 让 edit delta 边界显式化
- 让语法和语义重算可观测

### Stage B — 稳定语义核心

1. `M13` 稳定 HIR 与 item identity
2. `M14` 名字解析与 scope graph
3. `M15` 类型推断 queries

目的：

- 让语义身份在编辑下保持稳定
- 让 definition/reference/completion 消费真实语义结构

### Stage C — IDE 能力质量

1. `M16` completion engine 升级
2. `M17` workspace index

目的：

- 把补全从 token 启发式推进到类型和作用域驱动的排序
- 把 definition/references/workspace symbol 扩展到当前打开文件之外

### Stage D — 运行时硬化与收口

1. `M18` IDE runtime
2. `M19` 质量与性能收口

目的：

- 在负载下保持前台延迟稳定
- 在继续扩能力面之前锁定测试、fuzz 和性能门槛

---

## 7. 里程碑路线图

### 7.1 M11 — Multi-Edit Incremental Syntax

冻结验收：

- [`../milestones/2026-04-15/M11-MultiEditIncrementalSyntax.md`](../milestones/2026-04-15/M11-MultiEditIncrementalSyntax.md)

目标：

1. 保留 LSP 有序 edit
2. 让 VFS 成为 canonical text application 的唯一所有者
3. 让 Tree-sitter 直接消费结构化多段 delta

关键产物：

- `TextEdit` 和 `DocumentDelta`
- `didChange -> VFS -> SyntaxParser` delta 路径
- 多段增量复用和 fallback 层级

### 7.2 M12 — Semantic Query Cache

冻结验收：

- [`../milestones/2026-04-15/M12-SemanticQueryCache.md`](../milestones/2026-04-15/M12-SemanticQueryCache.md)

目标：

1. 用显式 query 替换粗粒度 `IdeSnapshot` 重算
2. 拆分文件级和 offset 级缓存
3. 明确 snapshot 变化时的失效规则

关键产物：

- `FileVersionKey` 和 `OffsetKey`
- 显式 query 家族
- cache instrumentation 和 invalidation rules

### 7.3 M13 — Stable HIR and Item Identity

冻结验收：

- [`../milestones/2026-04-15/M13-StableHIRAndItemIdentity.md`](../milestones/2026-04-15/M13-StableHIRAndItemIdentity.md)

目标：

1. 把语义真相 lower 成稳定 HIR，而不是 token 驱动摘要
2. 为顶层 item、scope 和 local 提供稳定 ID
3. 让未受影响的 item 在编辑后保留身份，而不是整体重绑

关键产物：

- canonical `ModuleId`、`ItemId`、`ScopeId`、`TypeId`、`SymbolId`
- 函数、import、local、block、resource 的 AST-to-HIR lowering
- 未改动 item 的稳定身份规则

### 7.4 M14 — Name Resolution and Scope Graph

冻结验收：

- [`../milestones/2026-04-15/M14-NameResolutionAndScopeGraph.md`](../milestones/2026-04-15/M14-NameResolutionAndScopeGraph.md)

目标：

1. 在显式词法作用域和模块作用域上解析名字
2. 一致地建模遮蔽、import、builtin 和跨文件查找
3. 去掉仍依赖纯文本匹配的 definition/reference 行为

关键产物：

- scope graph
- import 和 builtin resolver
- symbol 到 definition/reference 的映射

### 7.5 M15 — Type Inference Queries

冻结验收：

- [`../milestones/2026-04-15/M15-TypeInferenceQueries.md`](../milestones/2026-04-15/M15-TypeInferenceQueries.md)

目标：

1. 把语义缓存粒度下探到 whole-file 以下
2. 分离签名推断和函数体推断
3. 尽可能只让被编辑 item 失效

关键产物：

- per-item inference queries
- body hash 或等价 invalidation key
- 可被 completion 和 hover 消费的 receiver type / expected type 数据

### 7.6 M16 — Completion Engine Upgrade

冻结验收：

- [`../milestones/2026-04-15/M16-CompletionEngineUpgrade.md`](../milestones/2026-04-15/M16-CompletionEngineUpgrade.md)

目标：

1. 让 completion 同时依赖 syntax position、scope、receiver type 和 call-argument context
2. 一致地排序 locals/imports/builtins/snippets
3. 在语法错误下仍保持 best-effort completion

关键产物：

- 更丰富的 `CompletionContext`
- receiver-aware/member-aware ranking
- type position 和 call-site filtering

### 7.7 M17 — Workspace Index

冻结验收：

- [`../milestones/2026-04-15/M17-WorkspaceIndex.md`](../milestones/2026-04-15/M17-WorkspaceIndex.md)

目标：

1. 增加显式的 open-file、background 和 persistent 索引层
2. 加速 workspace symbol、跨文件 definition 和 references
3. 把 index 新鲜度和前台延迟解耦

关键产物：

- index schema 和 merge policy
- background indexing queue
- 持久化 symbol/reference store

### 7.8 M18 — IDE Runtime

冻结验收：

- [`../milestones/2026-04-15/M18-IDERuntime.md`](../milestones/2026-04-15/M18-IDERuntime.md)

目标：

1. 对 semantic diagnostics 做 debounce
2. 取消过期前台请求
3. 让可见文档工作优先于后台维护

关键产物：

- request cancellation / version guards
- 后台任务调度与优先级
- runtime counters 和 latency instrumentation

### 7.9 M19 — Quality and Performance Closure

冻结验收：

- [`../milestones/2026-04-15/M19-QualityAndPerformanceClosure.md`](../milestones/2026-04-15/M19-QualityAndPerformanceClosure.md)

目标：

1. 冻结 corpus、fuzz 和性能门槛
2. 让 Tree-sitter 与 Nightly parser 的结构漂移持续可见
3. 在延迟和稳定性目标达成前，阻止继续扩功能面

关键产物：

- syntax/semantic drift corpus
- syntax、completion 和 LSP sync 的 fuzz target
- benchmark 和回归 harness

---

## 8. 依赖顺序

实现顺序固定为：

`M11 -> M12 -> M13 -> M14 -> M15 -> M16 -> M17 -> M18 -> M19`

原因：

1. `M11` 先显式化 edit 边界
2. `M12` 再显式化 query 边界
3. `M13-M15` 让语义身份、解析和推断稳定到足以支撑高质量 IDE 功能
4. `M16` 依赖这些语义基础完成过滤和排序
5. `M17` 把同一套语义真相扩展到整个 workspace
6. `M18-M19` 用来硬化运行时行为并保持系统可度量

不要在不先更新本计划、再回写冻结 milestone 批次的前提下重排这个顺序。

---

## 9. 需要固定的公共形状

在整个路线图中，以下内部/公共边界保持固定：

```cpp
struct FileVersionKey { FileId file_id; SnapshotId snapshot_id; };
struct OffsetKey { FileId file_id; SnapshotId snapshot_id; std::size_t offset; };

struct CompletionContext {
  FileId file_id;
  SnapshotId snapshot_id;
  std::size_t offset;
  std::string prefix;
  PositionKind position_kind;
  TokenSet expected_tokens;
  CategorySet expected_categories;
  ScopeId scope_id;
  TypeId receiver_type_id;
};
```

到本计划结束时，必须具备的 query 家族：

1. `syntax_tree`
2. `semantic_summary`
3. `hir_module`
4. `scope_graph`
5. `resolve_name`
6. `infer_type`
7. `document_symbols`
8. `semantic_tokens`
9. `completion_context`
10. `completion`
11. `hover`
12. `definition`
13. `references`
14. `workspace_symbols`

---

## 10. 不在本轮范围内

以下内容仍不在本路线图范围内：

1. 以浏览器内 WASM 作为主 IDE 执行路径
2. 远程多租户语义服务
3. 在 `M19` 前把 rename / code action / inlay hint 作为一等交付物
4. 多根工作区语义
5. 协同编辑冲突处理

这些内容可以在以后规划，但只能在当前路线图完成之后。

---

## 11. 测试与性能门槛

每个里程碑都有自己的冻结验收，但到 `M19` 为止，如果下面这些顶层门槛还没有达成，这条路线图就不能视为完成：

1. 5k 行文件，热增量 syntax parse：`p95 <= 10ms`
2. 5k 行文件，热 completion：`p95 <= 50ms`
3. 5k 行文件，热 hover/definition：`p95 <= 80ms`
4. 当前仓库规模下，首次 background index：`<= 5s`
5. 冻结 syntax corpus 上不能存在未经明确豁免的 parser drift
6. syntax、completion 和 LSP sync 的 fuzz target 必须在 CI 中稳定通过

在这些门槛变绿，或在新计划中被明确豁免之前，不应继续扩展本路线图之外的 IDE 功能面。
