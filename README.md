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

> **Styio** is a symbolic programming language for stream processing, resource scheduling, and intent expression. It uses compact symbols for data flow and resource topology, with an LLVM-based backend for native execution paths.

## ✨ Why Styio?

| Feature | Description |
| :--- | :--- |
| 🌊 **Stream & Pulse Oriented** | First-class primitives for continuous data streams, discrete pulses, state mutations, and resource topologies. |
| 🔣 **Pure Symbolic Abstraction** | Minimizes keyword boilerplate in favor of expressive data flow. Styio relies on a direct symbolic system (`:=`, `->`, `>>`, `?=`) that visually aligns with actual data and resource flows. |
| ⚡ **LLVM-backed Execution** | Lowers compiler-accepted programs through the LLVM infrastructure and JIT execution path. |
| 🧩 **Ecosystem Interfaces** | Developed alongside **Spio** (package manager) and **Styio-view** (visualizer), with repository contracts documented in this project. |

## 🪄 A Glimpse of Styio

The example below is a runnable parallel-job signal. It reads two prices from `@stdin`, launches two independent jobs, awaits their results, and combines them into one output event.

```styio
price_a, price_b <- @stdin : (f64, f64)

||> [
    spread_job := { <| price_a - price_b }
    midpoint_job := { <| (price_a + price_b) / 2.0 }
]

?| spread_job -> spread: f64 | 0.0
?| midpoint_job -> midpoint: f64 | 0.0

?(spread > 5.0 || spread < -5.0) => {
    signal = ("parallel signal: spread=" + spread) + ", midpoint=" + midpoint
    signal -> @stdout
}
```

Run it from the repository root:

```bash
printf '101\n94\n' | build/default/bin/styio --file example/job_parallel_signal.styio
```

Expected output:

```text
parallel signal: spread=7.000000, midpoint=97.500000
```

## 🚀 Quick Start

```bash
curl -fsSL https://styio.io/tools/spio/install-spio.sh | sh -s -- --base-url https://styio.io
spio install styio@latest --prebuilt-only
styio --version
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
| ⚙️ **[`styio`](https://github.com/SymPolicy/Styio)** | **Core language, compiler, CLI, and SSOT** *(This repo)* | 🟢 Active |
| 📦 **[`styio-spio`](https://github.com/SymPolicy/styio-spio)** | Official package manager & cloud registry backend | 🟢 Active |
| 🖥️ **[`styio-view`](https://github.com/SymPolicy/styio-view)** | User-facing visual execution & editor UI | 🟢 Active |
| ☁️ **[`styio-platform`](https://github.com/SymPolicy/styio-platform)** | Platform products & hosted surface integration | 🟡 Bootstrap |
| 📖 **[`styio-book`](https://github.com/SymPolicy/styio-book)** | Product vision & whitepaper | 🟢 Active |
| 🛡️ **[`audit`](https://github.com/SymPolicy/audit)** | Centralized audit framework & security modules | 🟢 Active |
| 🛠️ **[`Unka-Malloc-doc`](https://github.com/SymPolicy/Unka-Malloc-doc)** | Cross-repository developer guides & workflow docs | 🟢 Active |
| 💻 **[`Unka-Malloc-env`](https://github.com/SymPolicy/Unka-Malloc-env)** | Standardized dev environments & toolchains | 🟢 Active |
| 🔌 **[`styio-ext-vsc`](https://github.com/SymPolicy/styio-ext-vsc)** | Official VS Code extension | 🟢 Active |
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
