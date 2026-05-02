# Styio

Styio 是一个面向流式处理、资源调度与意图表达的符号化语言。它试图用更少的自然语言关键字、更直接的符号系统和更贴近数据流/资源流的抽象，表达高性能的数据处理逻辑。

当前仓库是 Styio 的主仓库，承载语言与编译器实现本体。

> Status: Compiler-side baseline is live, but the ecosystem is not closed yet. `styio-nightly` 已完成 compiler / source-build 双通道基线与统一文档门禁；跨仓交付、云执行、移动端与 hosted product closure 仍按 `docs/plans/Styio-Ecosystem-Delivery-Master-Plan.md` 推进。

## Styio 是什么

- 一门面向流、脉冲、状态和资源拓扑的语言。
- 一套基于 LLVM 的编译器实现。
- 一个正在形成中的生态，包括包管理、开发文档、标准环境、示例工程、可视化和编辑器扩展。

## 快速感受

如果你已经在本地构建了 `styio`，可以直接试一下这个示例：

```bash
./example/cli_calculator.sh
```

进入交互模式后可以直接输入表达式：

```text
styio-calc> 1 + 2 * (3 + 4)
= 15
styio-calc> quit
```

交互模式支持上下方向键回放历史，默认历史文件位于
`$XDG_STATE_HOME/styio/calculator_history`，若未设置 `XDG_STATE_HOME`，则回退到
`~/.local/state/styio/calculator_history`。

也可以单次执行：

```bash
./example/cli_calculator.sh "1 + 2 * (3 + 4)"
```

这个脚本会临时生成如下形状的 Styio 程序：

```styio
>_(1 + 2 * (3 + 4))
```

输出应为：

```text
15
```

如果你想直接运行一个仓库内的基础样例：

```bash
./build/default/bin/styio --file tests/milestones/m1/t01_int_arith.styio
```

## 生态入口

以下清单来自 SymPolicy 组织当前公开的 Styio 相关仓库，最近核对时间为 2026-04-24。
职责边界的权威说明见 [docs/specs/REPOSITORY-MAP.md](docs/specs/REPOSITORY-MAP.md)。

| Repository | Role | Status |
| --- | --- | --- |
| [Styio](https://github.com/SymPolicy/Styio) | 官方语言与编译器源仓，承载语言设计、编译器实现、CLI、测试与主文档入口 | Active |
| [styio-platform](https://github.com/SymPolicy/styio-platform) | 平台级产品/hosted surface 的整合入口；当前仍是轻量 bootstrap 仓库 | Active bootstrap |
| [styio-spio](https://github.com/SymPolicy/styio-spio) | 包管理器、registry/cloud backend、repo-hosted control console | Active |
| [audit](https://github.com/SymPolicy/audit) | 集中审计框架与 Styio audit modules | Active |
| [Unka-Malloc-doc](https://github.com/SymPolicy/Unka-Malloc-doc) | 开发者手册与 GitBook 文档源 | Active |
| [styio-view](https://github.com/SymPolicy/styio-view) | 面向用户的编辑器、运行视窗与前端 adapter | Active |
| [Unka-Malloc-env](https://github.com/SymPolicy/Unka-Malloc-env) | 标准开发环境、devcontainer 与 toolchain bootstrap | Active |
| [styio-example](https://github.com/SymPolicy/styio-example) | 示例工程集合 | Active bootstrap |
| [styio-book](https://github.com/SymPolicy/styio-book) | 产品白皮书、愿景与对外叙述 | Active |
| [styio-ext-vsc](https://github.com/SymPolicy/styio-ext-vsc) | VS Code 插件 | Active |
| [styio-deprecated](https://github.com/SymPolicy/styio-deprecated) | 旧实现历史归档；只作为迁移和考古参考 | Archived |

当前三仓并行开发切面固定为：

- `styio-nightly` 负责 compiler / language / managed-toolchain SSOT
- `styio-spio` 负责 package manager、registry/cloud backend、repo-hosted control console
- `styio-view` 负责面向用户的编辑与运行前端，并通过 adapter 消费后端/工具链合同

## 开发者入口

如果你想做以下事情：

- 构建 Styio 编译器
- 阅读开发文档
- 参与贡献
- 了解测试、文档规则和开发环境

先看仓库级入口 [docs/BUILD-AND-DEV-ENV.md](docs/BUILD-AND-DEV-ENV.md)。

更广泛的跨仓开发文档再看 [Unka-Malloc-doc](https://github.com/SymPolicy/Unka-Malloc-doc)。

这个主仓库的 `README.md` 现在只保留用户入口信息；开发者导向内容不再放在首页展开。

如果你想先理解 Styio 的项目级优先级、重构边界与净室重写许可，请看 [docs/specs/PRINCIPLES-AND-OBJECTIVES.md](docs/specs/PRINCIPLES-AND-OBJECTIVES.md)。

## 当前仓库包含什么

- `src/`：Styio 编译器与 CLI 实现
- `tests/`：里程碑、流水线、安全性与回归测试
- `example/`：最小样例与脚本
- `docs/`：主仓库内部设计、规格、ADR 与历史文档

## 进一步阅读

- 产品与愿景： [styio-book](https://github.com/SymPolicy/styio-book)
- 示例工程： [styio-example](https://github.com/SymPolicy/styio-example)
- 编辑器支持： [styio-ext-vsc](https://github.com/SymPolicy/styio-ext-vsc)
- 生态边界说明： [docs/specs/REPOSITORY-MAP.md](docs/specs/REPOSITORY-MAP.md)
- 并行开发边界： [docs/specs/ECOSYSTEM-REPO-SPLIT-AND-PARALLEL-DEV.md](docs/specs/ECOSYSTEM-REPO-SPLIT-AND-PARALLEL-DEV.md)
- 项目原则与目标： [docs/specs/PRINCIPLES-AND-OBJECTIVES.md](docs/specs/PRINCIPLES-AND-OBJECTIVES.md)
