# Styio Repository Map

**Purpose:** Define repository responsibilities and documentation ownership across the Styio ecosystem; use this map to route work, not to track release status or implementation completeness.

**Last updated:** 2026-10-01

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

**Inventory refresh:** The Styio, developer-docs, book, and examples repository
names and canonical URLs below were verified against SymPolicy on 2026-10-01.
Other accessory links retain their prior inventory entries pending individual
verification. Downstream development repositories keep their own boundaries.

---

## 3. Repository Inventory

| Repository | Role | Owns What | Does Not Own |
|------------|------|-----------|--------------|
| [`Styio`](https://github.com/SymPolicy/Styio) / development mirror [README.md](../../README.md) | Language and compiler | Language design, grammar, compiler, CLI, tests, primary technical docs | Package-manager product, editor UI, standalone teaching corpus |
| [`styio-platform`](https://github.com/eBioRing/styio-platform) | Registry and hosted platform | Registry/control plane, hosted workspace, cloud job, worker and hosted API; workers invoke `pafio build` | Language semantics, compiler implementation, Pafio client state |
| [`pafio-nightly`](https://github.com/Unka-Malloc/pafio-nightly) | Package and project build entry | Manifest/lock, resolution, cache, offline reproduction, metadata, sync/check/build/run/test, vendor/pack/publish client | Language semantics, compiler, registry server or hosted workers |
| [`styio-audit`](https://github.com/eBioRing/styio-audit) | External audit framework | Auditable-code framework, default and Styio-specific audit modules | Language semantics, acceptance tests, compiler implementation |
| [`styio-dev-doc`](https://github.com/SymPolicy/styio-dev-doc) | Contributor documentation | Cross-repository development guides, setup and collaboration | Independent language semantics or compiler acceptance criteria |
| [`styio-dev-env`](https://github.com/eBioRing/styio-dev-env) | Development environment | Devcontainer, toolchain bootstrap, shared CI/local setup | Language design or example programs |
| [`styio-book`](https://github.com/SymPolicy/styio-book) | Learning material and language narrative | Tutorials and explanatory chapters, each tied to its applicable language version | Authoritative current syntax, compiler internals or acceptance rules |
| [`vityo-nightly`](https://github.com/Unka-Malloc/vityo-nightly) | Visual IDE and execution presentation | Editor/workspace UI, runtime views and toolchain adapters | Compiler semantics, package rules or platform backend |
| [`styio-examples`](https://github.com/SymPolicy/styio-examples) | Generated algorithm gallery | Presentation and indexing of versioned source examples | A second syntax definition or independent compiler test oracle |
| [`styio-ext-vsc`](https://github.com/eBioRing/styio-ext-vsc) | VS Code extension | Highlighting, snippets, editor interaction and language-service integration | Language semantics or compiler behavior |
| [`styio-deprecated`](https://github.com/eBioRing/styio-deprecated) | Historical implementation archive | Historical code and migration references | Active syntax, current tests or development entrypoints |

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

### 4.3 Cross-Links and Source Authority

- `styio-dev-doc` explains contributor workflows and links to compiler contracts.
- `styio-examples` presents code from a declared source revision and preserves
  its source links rather than maintaining a separate algorithm corpus.
- `styio-ext-vsc` documents editor integration and consumes the language grammar.
- `styio-book` provides explanations and tutorials. A chapter identifies its
  compiler version and executable evidence; historical chapters are not an
  active-syntax authority.
- `styio-audit` owns external audit execution. Accepted findings return to the
  owning compiler source, tests, and technical documents.

### 4.4 Maintenance and Programming Skills

Compiler-maintenance skills live in `workflows/skills/`, use `skill.toml`, and
are checked by `scripts/tool-skill-registry-gate.py`. They route contributors
to source owners, feature contracts, tests, and delivery workflows.

An installable Styio programming skill belongs in a separate repository. It
teaches application authors accepted syntax, idioms, composition, diagnostics,
and compiler-version compatibility. Its examples use version-pinned language
contracts and executable fixtures. Distribution location and publication policy
are decisions of that repository, not compiler runtime capabilities.

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

## 6. Maintenance

1. Verify a repository's canonical name and URL before changing its entry; use
   the current SymPolicy inventory and preserve downstream development links.
2. Update the [documentation policy](./DOCUMENTATION-POLICY.md) authority table
   when a repository assumes a maintained documentation responsibility.
3. Keep implementation and publication status in the owning repository. This
   map records stable responsibilities rather than a live delivery dashboard.
4. Change repository roles explicitly when relocating content; a new teaching
   or tooling package does not become a language-semantics authority.
