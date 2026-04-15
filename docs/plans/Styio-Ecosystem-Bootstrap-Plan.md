# Styio Ecosystem Bootstrap Plan

**Purpose:** 为 Styio 的配件仓库提供统一的**启动蓝图**与**最小交付清单**，明确每个仓库第一阶段至少要补哪些文档、目录和能力；本文件是执行计划，**不是**语言语义、编译器行为或任一配件仓库的长期 SSOT。

**Last updated:** 2026-04-10

**Repository boundary SSOT:** [`../specs/REPOSITORY-MAP.md`](../specs/REPOSITORY-MAP.md)  
**Docs policy:** [`../specs/DOCUMENTATION-POLICY.md`](../specs/DOCUMENTATION-POLICY.md)  
**Main compiler repo entry:** [`../../README.md`](../../README.md)

---

## 1. 背景

当前 Styio 已经形成“主仓库 + 配件仓库”的生态结构，但除 `styio` 主仓库外，绝大多数配件仓库仍处于以下状态之一：

- 仅有仓库占位，尚未形成稳定边界。
- 已有零散内容，但 README、目录结构和文档入口不足。
- 文档和代码状态明显滞后，难以判断仓库当前应承担的职责。

因此，现阶段最重要的不是立即把每个配件都“做完”，而是先完成三件事：

1. 让每个仓库的**角色边界**清楚。
2. 让每个仓库的**入口文档**可读、可发现、可交接。
3. 让主仓库 `styio` 与周边仓库之间形成**稳定的引用关系**，避免多份语义和规范并行漂移。

---

## 2. 范围与非目标

### 2.1 本计划覆盖

- `styio-spio`
- `Unka-Malloc-doc`
- `Unka-Malloc-env`
- `styio-book`
- `styio-view`
- `styio-examples`
- `styio-ext-vsc`

### 2.2 本计划不覆盖

- Styio 语言语义和编译器设计的重新定义。
- 配件仓库的完整功能实现细节。
- 每个仓库的实时 issue、燃尽图、发布节奏。
- 将主仓库中的设计文档整体复制到其他仓库。

---

## 3. 全仓库统一基线

无论哪个配件仓库，第一阶段都应先补齐以下基线。

### 3.1 最低目录与文件

| Item | 要求 |
|------|------|
| `README.md` | 必须存在，说明仓库用途、当前状态、与 `styio` 主仓库的关系、下一步计划 |
| `LICENSE` | 必须明确 |
| `.gitignore` | 至少覆盖该仓库主要技术栈的构建产物 |
| `docs/` 或等价文档目录 | 只要仓库内容超过单页 README，就应拆出文档目录 |
| `examples/` 或 `sample/` | 对需要演示的仓库优先提供最小示例 |
| `CHANGELOG.md` 或 Release Policy | 可后置，但在进入公开迭代前要补上 |

### 3.2 README 最低结构

每个配件仓库的 `README.md` 至少应包含：

1. 仓库定位。
2. 当前状态。
3. 与 `styio` 主仓库的关系。
4. 最小使用方式或未来使用方式。
5. 文档入口。
6. 开发入口或待办方向。

建议统一加一段显式状态说明：

> Status: Early bootstrap / partial / stale docs expected

这样可以避免外部读者把占位仓库误认成已稳定可用的产品。

### 3.3 与主仓库的同步规则

1. 语言语义、语法、编译器行为，一律链接回 `styio/docs/design/`。
2. 配件仓库只维护自己的局部权威，不复制主仓库 SSOT。
3. 如果配件仓库需要引用 Styio 语法示例，应注明“示例以主仓库当前实现为准”。
4. 当主仓库发生边界变化时，先更新 [`../specs/REPOSITORY-MAP.md`](../specs/REPOSITORY-MAP.md)，再同步配件仓库入口文档。

---

## 4. 分阶段启动策略

### Phase 0: 可识别

目标：让每个仓库“看起来像一个真实项目”，而不是空壳。

交付：

- 仓库 `README.md`
- 状态说明
- 最低目录结构
- 指向 `styio` 主仓库的链接

### Phase 1: 可理解

目标：让新读者知道这个仓库将来要做什么、怎么和主仓库配合。

交付：

- 文档目录入口
- 设计边界说明
- 初始里程碑或 roadmap
- 最小示例或截图

### Phase 2: 可试用

目标：让仓库至少有一个能运行、能截图、能验证的最小能力。

交付：

- 一条最小工作流
- 一份验证说明
- 与主仓库版本或能力的兼容说明

---

## 5. 按仓库的最小交付清单

### 5.1 `styio-spio`

目标：定义 Styio 包管理器的存在价值，先补“包是什么”和“以后如何装/发/解依赖”。

Phase 0：

- `README.md` 说明它是 Styio 的包管理器，而不是编译器本体。
- 声明当前是否仅为设计占位。
- 链接回 `styio` 主仓库。

Phase 1：

- `docs/package-format.md`：包元数据长什么样。
- `docs/registry-model.md`：包源、索引、镜像、缓存的大致模型。
- `docs/cli-sketch.md`：例如 `spio init/install/publish/search` 的命令草图。

Phase 2：

- 一个最小本地包安装流程。
- 一个最小 `styio-examples` 集成示例。

### 5.2 `Unka-Malloc-doc`

目标：承接跨仓库开发说明，而不是替代主仓库设计文档。

Phase 0：

- `README.md` 明确它是“开发者手册仓库”。
- 说明语言与编译器 SSOT 仍在 `styio` 主仓库。

Phase 1：

- `docs/getting-started.md`：开发者如何从零了解 Styio 生态。
- `docs/build-styio.md`：如何构建主仓库。
- `docs/write-tests.md`：如何新增 milestone/test cases。
- `docs/write-docs.md`：如何遵守 `styio` 主仓库文档规则。

Phase 2：

- 跨仓库开发流程图。
- 一份面向外部贡献者的完整 onboarding 路径。

### 5.3 `Unka-Malloc-env`

目标：提供可复用、可验证的标准开发环境。

Phase 0：

- `README.md` 说明支持哪些平台、目标用户是谁。
- 说明它服务于哪些仓库。

Phase 1：

- `devcontainer/`、Dockerfile、bootstrap script 或等价结构。
- `docs/toolchain-matrix.md`：LLVM、CMake、编译器、Node/Python 等版本矩阵。
- `docs/verify-environment.md`：环境就绪后如何验证。

Phase 2：

- 一条命令拉起 Styio 主仓库开发环境。
- CI 或本地脚本验证环境完整性。

### 5.4 `styio-book`

目标：承载产品叙事与白皮书，而不是工程实现规范。

Phase 0：

- `README.md` 说明其面向的读者是产品、研究或潜在用户。
- 说明技术细节仍应回链主仓库。

Phase 1：

- `chapters/01-vision.md`
- `chapters/02-problem-space.md`
- `chapters/03-why-symbolic-stream-language.md`
- `chapters/04-ecosystem-overview.md`

Phase 2：

- 一版可连续阅读的白皮书草稿。
- 与 `styio-view` 的展示内容保持基础一致。

### 5.5 `styio-view`

目标：提供对外展示和可视化承载，不与主仓库争夺技术权威。

Phase 0：

- `README.md` 说明它是官网/展示页/可视化入口中的哪一种。
- 说明数据和描述是否来自主仓库生成或手工维护。

Phase 1：

- 页面信息架构文档。
- 路由或页面草图。
- 品牌与文案边界说明。

Phase 2：

- 一个可部署的首页。
- 至少一个可视化 Styio 示例或编译链展示页面。

### 5.6 `styio-examples`

目标：集中管理“可运行示例”，避免主仓库把所有教学案例长期塞在 `tests/` 和 `sample/` 中。

Phase 0：

- `README.md` 说明它不是测试仓库，而是面向学习和演示的示例仓库。
- 说明示例与主仓库测试样例的差异。

Phase 1：

- 按主题建目录，例如 `basic/`、`io/`、`stream/`、`state/`。
- 每个示例有独立 README、输入、预期输出。
- 指向对应的主仓库 milestone 或设计章节。

Phase 2：

- 提供至少 3 个可直接运行的 canonical examples。
- 尝试与 `styio-spio` 的未来包结构兼容。

### 5.7 `styio-ext-vsc`

目标：先做编辑器接入层，再视情况扩展为语言服务或调试能力。

Phase 0：

- `README.md` 说明目前支持的功能边界。
- 清楚区分“语法高亮”“片段”“语言服务”“调试”四类能力。

Phase 1：

- `syntaxes/` 或等价 grammar 文件。
- `snippets/`。
- `docs/settings.md` 与 `docs/roadmap.md`。

Phase 2：

- 最小可安装插件。
- 至少支持语法高亮和基础文件识别。

---

## 6. 建议的统一交接模板

为减少配件仓库 README 风格发散，建议每个仓库先使用同一套最小模板：

```markdown
# <repo-name>

一句话说明这个仓库负责什么。

> Status: Early bootstrap / partial / stale docs expected

## Scope
- 负责什么
- 不负责什么

## Relationship to `styio`
- 主仓库地址
- 哪些内容仍以主仓库为准

## Current State
- 已有内容
- 缺失内容

## Next Steps
- 第一阶段准备补什么

## Docs
- 文档入口
```

---

## 7. 建议的落地顺序

如果一次只能推进少量仓库，建议优先顺序如下：

1. `Unka-Malloc-doc`
2. `Unka-Malloc-env`
3. `styio-examples`
4. `styio-ext-vsc`
5. `styio-spio`
6. `styio-view`
7. `styio-book`

原因：

- 前四者最直接影响开发者上手和生态可感知度。
- `styio-spio` 很重要，但如果语言与示例都没稳定，包管理器设计容易空转。
- `styio-view` 和 `styio-book` 偏展示与叙事，可以晚于开发基础设施启动。

---

## 8. 在主仓库中的后续动作

为了让这份计划能真正被使用，主仓库后续至少还应完成：

1. 在 `styio` 主仓库中继续维护 [`../specs/REPOSITORY-MAP.md`](../specs/REPOSITORY-MAP.md)。
2. 当某个配件仓库开始建设时，把它的 README 或 docs 入口对齐到本计划的最小结构。
3. 当某个配件仓库形成稳定权威边界时，再决定是否把其入口登记为新的跨仓库 SSOT。
4. 不在主仓库中维护逐仓库实时进度表，避免再次形成过期状态说明。

---

## 9. 完成定义

某个配件仓库可以被视为“完成 bootstrap”，当且仅当：

1. 仓库定位清楚。
2. README 可独立阅读。
3. 与 `styio` 主仓库的关系清楚。
4. 至少有一个文档入口或最小示例。
5. 外部读者不会再把它误判为“主仓库的重复品”或“功能已经完整可用的产品”。
