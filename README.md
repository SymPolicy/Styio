# Styio

Styio 是一个面向流式处理、资源调度与意图表达的符号化语言。它试图用更少的自然语言关键字、更直接的符号系统和更贴近数据流/资源流的抽象，表达高性能的数据处理逻辑。

当前仓库是 Styio 的主仓库，承载语言与编译器实现本体。

> Status: Early stage. Styio 仍处于快速演进阶段，生态仓库多数仍在启动或补文档阶段。

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
./build/bin/styio --file tests/milestones/m1/t01_int_arith.styio
```

## 生态入口

| Repository | Role |
| --- | --- |
| [styio-spio](https://github.com/SymPolicy/styio-spio) | 包管理器 |
| [Unka-Malloc-doc](https://github.com/SymPolicy/Unka-Malloc-doc) | 开发者文档 |
| [Unka-Malloc-env](https://github.com/SymPolicy/Unka-Malloc-env) | 标准开发环境 |
| [styio-book](https://github.com/SymPolicy/styio-book) | 产品白皮书 |
| [styio-view](https://github.com/SymPolicy/styio-view) | 可视化页面 |
| [styio-examples](https://github.com/SymPolicy/styio-examples) | 示例工程集合 |
| [styio-ext-vsc](https://github.com/SymPolicy/styio-ext-vsc) | VS Code 插件 |

## 开发者入口

如果你想做以下事情：

- 构建 Styio 编译器
- 阅读开发文档
- 参与贡献
- 了解测试、文档规则和开发环境

请直接前往 [Unka-Malloc-doc](https://github.com/SymPolicy/Unka-Malloc-doc)。

这个主仓库的 `README.md` 现在只保留用户入口信息；开发者导向内容不再放在首页展开。

如果你想先理解 Styio 的项目级优先级、重构边界与净室重写许可，请看 [docs/specs/PRINCIPLES-AND-OBJECTIVES.md](docs/specs/PRINCIPLES-AND-OBJECTIVES.md)。

## 当前仓库包含什么

- `src/`：Styio 编译器与 CLI 实现
- `tests/`：里程碑、流水线、安全性与回归测试
- `example/`：最小样例与脚本
- `docs/`：主仓库内部设计、规格、ADR 与历史文档

## 进一步阅读

- 产品与愿景： [styio-book](https://github.com/SymPolicy/styio-book)
- 示例工程： [styio-examples](https://github.com/SymPolicy/styio-examples)
- 编辑器支持： [styio-ext-vsc](https://github.com/SymPolicy/styio-ext-vsc)
- 生态边界说明： [docs/specs/REPOSITORY-MAP.md](docs/specs/REPOSITORY-MAP.md)
- 项目原则与目标： [docs/specs/PRINCIPLES-AND-OBJECTIVES.md](docs/specs/PRINCIPLES-AND-OBJECTIVES.md)
