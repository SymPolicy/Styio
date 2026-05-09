<div align="center">

# ✨ Styio

**A symbolic language for stream processing, resource scheduling, and intent expression.**

[![Build Status](https://img.shields.io/github/actions/workflow/status/styio-org/styio-nightly/build.yml?style=for-the-badge&logo=github&color=2ea44f)](https://github.com/SymPolicy/styio/actions)
[![Release](https://img.shields.io/github/v/release/styio-org/styio-nightly?style=for-the-badge&color=0366d6)](https://github.com/styio-org/styio-nightly/releases)
[![License](https://img.shields.io/github/license/styio-org/styio-nightly?style=for-the-badge&color=blue)](https://github.com/styio-org/styio-nightly/blob/main/LICENSE)

<br/>

[**Read the Vision**](https://github.com/SymPolicy/styio-book) • [**Quick Start**](#-quick-start) • [**Documentation**](https://github.com/SymPolicy/Unka-Malloc-doc) • [**🇨🇳 简体中文**](README_zh.md)

</div>

---

> **Styio** is a high-performance, symbolic programming language engineered from the ground up for modern data flows. By stripping away verbose natural language keywords and introducing direct, math-like symbolic abstractions, Styio allows you to express complex stream pipelines and resource topologies with unprecedented clarity. Powered by an **LLVM-based backend**, it delivers native performance for the most demanding execution environments.

## ✨ Why Styio?

| Feature | Description |
| :--- | :--- |
| 🌊 **Stream & Pulse Oriented** | First-class primitives for continuous data streams, discrete pulses, state mutations, and resource topologies. |
| 🔣 **Pure Symbolic Abstraction** | Minimizes keyword boilerplate in favor of expressive data flow. Styio relies on a direct symbolic system (`:=`, `->`, `>>`, `?=`) that visually aligns with actual data and resource flows. |
| ⚡ **Bare-Metal Performance** | Compiles to highly optimized machine code via the LLVM infrastructure, leveraging JIT compilation for zero-overhead execution. |
| 🧩 **Holistic Ecosystem** | Designed alongside **Spio** (package manager) and **Styio-view** (visualizer) for a seamless, end-to-end developer experience. |

## 🪄 A Glimpse of Styio

Styio's syntax is built to make complex stateful logic feel intuitive. The following **"Golden Cross"** example represents the constitution of Styio's language design: defining a resource, processing streams, managing state, and triggering intent.

```styio
// 1. Declare persistent top-level resources with historical bounds
@ma5 : f64|..5|
@ma20 : f64|..20|

// 2. Monitor a continuous market data stream
@market{"ASSET_X"} >> #(p) => {
    
    # get_ma := (src, n) => src[avg, n]

    // 3. Write into resource sinks directly
    get_ma(p, 5)  -> @ma5
    get_ma(p, 20) -> @ma20

    // 4. Symbolic condition: check for a golden cross using historical selectors
    is_golden = cross_over(@ma5[-1], @ma20[-1])

    // Define the execution intent
    # order_logic := (price) => { >_ ("Buy at: " + price) }

    // 5. Guarded wave execution block
    ?(is_golden) => {
        order_logic(p)
    }
}
```
*Notice the absence of traditional keywords. Every symbol dictates the exact flow of data and execution.*

## 🚀 Quick Start

Use the interactive CLI calculator to try Styio with the same compiler engine that parses and executes expressions.

```bash
# 1. Clone the repository
git clone https://github.com/SymPolicy/Styio.git
cd Styio

# 2. Run the interactive calculator
./example/cli_calculator.sh
```

```text
styio-calc> 1 + 2 * (3 + 4)
= 15
styio-calc> quit
```

## 🛠️ Building & Development

Styio is an active open-source project. If you want to build the compiler from source or contribute to its core, check out our comprehensive environment guide:

👉 **[BUILD-AND-DEV-ENV.md](docs/BUILD-AND-DEV-ENV.md)**

*(For broader cross-repository development guides, visit the [Unka-Malloc-doc](https://github.com/SymPolicy/Unka-Malloc-doc) repository).*

## 🧭 Project Ecosystem & Status

> **Core Status: Compiler Baseline Active** 🟢

The compiler-side baseline is live, and we are rapidly expanding the surrounding ecosystem according to our `Styio-Ecosystem-Delivery-Master-Plan.md`. Here is the current map of the official Styio universe:

| Repository | Ecosystem Role | Status |
| :--- | :--- | :--- |
| ⚙️ **[`Styio`](https://github.com/SymPolicy/Styio)** | **Core language, compiler, CLI, and SSOT** *(This repo)* | 🟢 Active |
| 📦 **[`styio-spio`](https://github.com/SymPolicy/styio-spio)** | Official package manager & cloud registry backend | 🟢 Active |
| 🖥️ **[`styio-view`](https://github.com/SymPolicy/styio-view)** | User-facing visual execution & editor UI | 🟢 Active |
| 📖 **[`styio-book`](https://github.com/SymPolicy/styio-book)** | Product vision & whitepaper | 🟢 Active |
| 🛠️ **[`Unka-Malloc-doc`](https://github.com/SymPolicy/Unka-Malloc-doc)** | Cross-repository developer guides & workflow docs | 🟢 Active |
| 🛡️ **[`audit`](https://github.com/SymPolicy/audit)** | Centralized audit framework & security modules | 🟢 Active |
| 💻 **[`Unka-Malloc-env`](https://github.com/SymPolicy/Unka-Malloc-env)** | Standardized dev environments & toolchains | 🟢 Active |
| 🔌 **[`styio-ext-vsc`](https://github.com/SymPolicy/styio-ext-vsc)** | Official VS Code extension | 🟢 Active |
| ☁️ **[`styio-platform`](https://github.com/SymPolicy/styio-platform)** | Platform products & hosted surface integration | 🟡 Bootstrap |
| 💡 **[`styio-example`](https://github.com/SymPolicy/styio-example)** | Example projects & reusable patterns | 🟡 Bootstrap |

## 📖 Further Reading

Dive deeper into the design, specifications, and architecture of Styio:

也可以把单个 Styio 源文件编译成本机可执行文件：

```bash
./build/bin/styio build tests/milestones/m10/t01_stdin_echo.styio -o /tmp/styio-stdin-echo
cat tests/milestones/m10/data/echo_input.txt | /tmp/styio-stdin-echo
```

*   **[Project Principles & Goals](docs/specs/PRINCIPLES-AND-OBJECTIVES.md)**
*   **[Repository Ecosystem Map](docs/specs/REPOSITORY-MAP.md)**
*   **[Parallel Development Specs](docs/specs/ECOSYSTEM-REPO-SPLIT-AND-PARALLEL-DEV.md)**
*   **[Product Vision & Whitepaper](https://github.com/SymPolicy/styio-book)**
*   **[Examples Collection](https://github.com/SymPolicy/styio-example)**
