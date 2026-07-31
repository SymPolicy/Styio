# Styio Repository Map

**Purpose:** 记录 Styio 官方仓库生态、各仓库的预期职责边界、以及文档应当落在哪个仓库；本文件用于说明“谁负责什么”，**不**负责追踪实时开发进度、版本发布状态或逐仓库完成度。

**Last updated:** 2026-07-30

---

## 1. 使用方式

当你需要回答以下问题时，先看本文件：

- Styio 官方现在有哪些仓库。
- 当前 `styio` 主仓库和周边配件仓库分别负责什么。
- 某类文档应该写在主仓库还是配件仓库。
- 多个仓库文档描述不一致时，应该优先相信哪里。

如果你关心的是语言语义、编译器行为或测试验收，请继续回到：

- [`../design/Styio-Language-Design.md`](../design/Styio-Language-Design.md)
- [`../design/Styio-EBNF.md`](../design/Styio-EBNF.md)
- [`./DOCUMENTATION-POLICY.md`](./DOCUMENTATION-POLICY.md)

如果你关心的是三仓如何并行推进、谁拥有哪类合同，请看：

- [`./ECOSYSTEM-REPO-SPLIT-AND-PARALLEL-DEV.md`](./ECOSYSTEM-REPO-SPLIT-AND-PARALLEL-DEV.md)

---

## 2. 总体说明

Styio 当前采用“主仓库 + 配件仓库”的生态结构：

- `Styio` 是官方语言与编译器源仓，承载语言、编译器、CLI、测试与主文档。
- 其余仓库围绕平台产品、包管理、审计、开发环境、产品文档、示例、编辑器扩展和可视化等方向展开。
- 除非某个配件仓库已明确建立并持续维护自己的权威边界，否则默认仍以 `styio` 主仓库中的设计与规格文档为准。

当前项目共识是：**主仓库之外的关键配件仓库已经有了清晰职责边界，但实现成熟度和交付闭合度仍不一致。** 因此，本文件优先解决“仓库角色识别”和“文档归属”，而不是给出实时状态看板。

**Inventory refresh:** 2026-04-24 使用 `gh repo list eBioRing --limit 200`
与 `gh search repos 'styio org:eBioRing'` 核对组织仓库。仓库的每周活跃度、
issue 状态和发布进度不在本文件维护。

---

## 3. 官方仓库清单

| Repository | Role | Owns What | Does Not Own |
|------------|------|-----------|--------------|
| [`Styio`](https://github.com/eBioRing/Styio) / 当前开发镜像 [`README.md`](../../README.md) | 主语言与编译器仓库 | 语言设计、形式文法、编译器实现、CLI、测试、主文档入口 | 包管理器、编辑器插件、产品白皮书、示例工程生态 |
| [`styio-platform`](https://github.com/eBioRing/styio-platform) | Registry 与 hosted/cloud 平台 | Registry 服务和控制面、hosted workspace、cloud job、worker、平台控制台与 hosted API；worker 调用 `pafio build` | 语言语义、编译器实现、Pafio 客户端与项目状态 |
| [`pafio-nightly`](https://github.com/Unka-Malloc/pafio-nightly) | 包管理与项目构建入口 | `pafio.toml`、`pafio.lock`、依赖解析、缓存与离线复现、metadata、sync、check/build/run/test、vendor、pack、publish 客户端 | Styio 核心语言语义与编译器实现、registry 服务端、hosted/cloud/worker |
| [`styio-audit`](https://github.com/eBioRing/styio-audit) | 集中审计框架 | auditable-code 框架、默认审计模块、Styio 专用审计模块与外部审计入口 | 编译器语义真相、语言接受测试、仓库本地代码实现 |
| [`styio-dev-doc`](https://github.com/eBioRing/styio-dev-doc) | 开发者文档仓库 | 跨仓库开发手册、搭建流程、协作说明、外部开发者上手指南 | 语言权威语义、编译器测试验收、产品白皮书 |
| [`styio-dev-env`](https://github.com/eBioRing/styio-dev-env) | 标准开发环境 | devcontainer、toolchain bootstrap、统一环境脚本、CI/本地环境约定 | 语言设计、产品定义、示例工程内容 |
| [`styio-book`](https://github.com/eBioRing/styio-book) | 产品白皮书 | 产品愿景、定位、理念叙事、对外说明材料 | 编译器行为细节、测试接受标准、工程实现规范 |
| [`vityo-nightly`](https://github.com/Unka-Malloc/vityo-nightly) | 面向用户的 Vityo IDE 与运行视窗前端 | 用户界面、编辑器壳层、运行视窗、面向人的工作区与交互；通过 adapter 消费工具链后端 | 编译器主实现、语言 SSOT、包管理规则、仓库/云平台后端语义 |
| [`styio-example`](https://github.com/eBioRing/styio-example) | 示例工程集合 | 可运行样例、模板项目、可复用示例模式 | 语言规范正文、编译器验收标准 |
| [`styio-ext-vsc`](https://github.com/eBioRing/styio-ext-vsc) | VS Code 插件 | 语法高亮、片段、编辑器交互、未来可能的语言服务集成 | 语言语义权威定义、编译器主行为 |
| [`styio-deprecated`](https://github.com/eBioRing/styio-deprecated) | 旧实现历史归档 | 历史代码、迁移参照、考古材料 | 当前语言语义、当前测试验收、活跃开发入口 |

---

## 4. 文档归属边界

### 4.1 仍然应该写在 `styio` 主仓库中的内容

- 语言语义、词法、文法、符号系统。
- 编译器前端、类型系统、IR、CodeGen、CLI 行为。
- 自动化测试、golden、里程碑冻结规格与验收路径。
- 贡献规范、agent 规则、文档维护策略。
- 与主编译器实现直接绑定的设计冲突、ADR、历史记录。

### 4.2 应该放到配件仓库自身的内容

- Pafio 命令、manifest/lock、依赖解析、离线状态与 publish 客户端合同。
- Styio Platform 的 registry/control-plane、hosted workspace、cloud job、worker 与控制台合同。
- 标准开发环境的镜像、依赖安装脚本、环境 bootstrap。
- VS Code 插件的命令、设置项、快捷键、发布说明。
- 可视化页面的前端交互、部署方式、页面信息架构。
- 示例工程的 README、项目模板、脚手架说明。
- 产品白皮书与对外叙述材料。
- 平台产品壳层、hosted surface、审计框架与外部审计执行说明。

### 4.3 可以双向链接，但不要双写的内容

- `styio-dev-doc` 可以解释如何开发 `styio`，但不应重新定义语言语义。
- `styio-example` 可以展示语法如何使用，但不应维护另一份语言规范。
- `styio-ext-vsc` 可以说明编辑器如何支持 Styio，但不应给出独立的语法真相版本。
- `styio-book` 可以叙述“为什么需要 Styio”，但不应替代编译器与设计文档的技术定义。
- `styio-audit` 可以执行外部审计和提供审计模块，但发现项进入当前仓库后仍要回到本仓源码、测试和 docs SSOT 落地。

---

## 5. 冲突时的优先级

当多个仓库的内容不一致时，按以下顺序处理：

1. **语言与语义问题**：优先以 `styio/docs/design/` 为准。
2. **文档规则、贡献流程、依赖边界**：优先以 `styio/docs/specs/` 为准。
3. **实现是否已接受某行为**：优先看 `styio` 主仓库中的源码与 `tests/`。
4. **配件仓库自己的实现细节**：由该配件仓库自身文档负责，但不得反向覆盖 `styio` 主仓库的语言 SSOT。

这意味着：

- 配件仓库可以拥有自己的局部权威。
- 但它们不能覆盖 `styio` 主仓库对语言、编译器和验收行为的定义。

---

## 6. 维护规则

1. 新增、重命名、归档 Styio 官方仓库时，先用 `gh repo list eBioRing --limit 200` 核对组织清单，再更新本文件和根目录 [`../../README.md`](../../README.md) 的生态列表。
2. 如果某个配件仓库开始承担稳定、可持续维护的文档权威边界，应同时更新 [`./DOCUMENTATION-POLICY.md`](./DOCUMENTATION-POLICY.md) 的 SSOT 表。
3. 如果某个仓库仍只处于占位或严重滞后状态，可以在其自身仓库中写状态说明，但不要在主仓库里维护实时进度表。
4. 本文件记录的是“结构与边界”，不是项目管理看板；不要把逐周进度、issue 清单、燃尽信息写进来。
