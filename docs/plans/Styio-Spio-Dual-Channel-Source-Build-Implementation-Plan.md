# Styio / Spio Dual-Channel Source-Build Implementation Plan

**Purpose:** 作为 `styio-nightly` 侧适配 `styio-spio` 双通道工具链模型的总实施计划，冻结 compiler-side contract、源码分层、默认符号层收敛、source-build 入口、测试与交付阶段顺序。当前 repo-local baseline 已完成；本文件保留在 `docs/plans/`，用于记录完成证据和后续 hardening/生态对齐边界，而不是替代 `docs/external/for-spio/` handoff contract 或 `spio` 仓库自己的 SSOT。

**Last updated:** 2026-06-28

**Plan status:** Repo-local baseline completed. Remaining ecosystem-wide closure is tracked in [Styio-Ecosystem-Delivery-Master-Plan.md](./Styio-Ecosystem-Delivery-Master-Plan.md) and the owning consumer plans.

> **MIGRATION-NEEDED: M-PLAN-01** ([../rollups/MIGRATION-LEDGER.md](../rollups/MIGRATION-LEDGER.md)). Once the master plan absorbs the residual hardening rules and the `spio` SSOT covers the cross-repo contract, archive this file through `docs/archive/ARCHIVE-MANIFEST.json` and remove it from the current tree.

## 前置条件

1. 并行: source-build hardening should split by binary contract, source-build contract, source layout, symbol registry, tests, docs, and consumer-handoff lanes. Only shared compiler service metadata, CMake/source-layout substrate, and docs-gate ownership changes are serial merge gates.
2. 子智能体: sub-agents may audit or prototype one lane at a time, but compiler-side source-build contract changes must be reconciled by the CLI / Nano and Docs / Ecosystem owners.
3. 基座: shared source-build metadata, docs gate, workflow, or CMake/source-layout substrate changes must land first through [Styio-Common-Foundation-Plan.md](./Styio-Common-Foundation-Plan.md).
4. Upstream dependency: remaining ecosystem closure is sequenced by [Styio-Ecosystem-Delivery-Master-Plan.md](./Styio-Ecosystem-Delivery-Master-Plan.md).

## Parallel Lane Map

| Lane | Parallel work allowed | Serial merge gate |
|------|-----------------------|-------------------|
| Binary contract | `--machine-info=json`, compile-plan receipt, diagnostics, and runtime-event compatibility audits. | Published payload fields or consumer-facing contract shape changes. |
| Source-build contract | `--source-build-info=json`, minimal source graph, toolchain metadata, and helper-script evidence. | Source-layout/CMake substrate changes shared by more than one consumer. |
| Symbol registry | Default symbol/builtin inventory, consumer references, and drift checks. | Compiler-owned registry schema or generated artifact shape changes. |
| Tests and gates | Negative-path discovery, docs gate dry runs, cross-repo handoff checks, and fixture inventory. | New required gate policy, generated index updates, and final acceptance evidence. |
| Ecosystem handoff | Spio/view confirmation questions, mirror-doc deltas, and migration-readiness notes. | Cross-repo SSOT wording, package/toolchain ownership, or release-channel policy. |

## 1. Goal

`styio-nightly` 需要同时服务 `spio` 的两条工具链通道：

1. `binary`
   - `spio use binary`
   - 消费已发布 `styio` 二进制
   - 继续通过 `--machine-info=json` 和 `--compile-plan <path>` 完成黑盒握手与执行
2. `build`
   - `spio use build`
   - 消费官方 `styio` 源码树
   - 通过 `--source-build-info=json` 获取稳定的 source-build contract
   - 目标是按官方受控子图分层构建 compiler/std-symbols/runtime，并最终执行 `minimal` 模式构建

这份计划的任务不是重新定义 `spio`，而是让 `styio-nightly` 以稳定、可测试、可交接的方式支撑这两条通道。

## 2. Scope

本计划覆盖：

1. `styio` 对 `spio` 的 binary/build 双通道 contract
2. `styio` 官方源码树的受控子图和 build 边界
3. 默认符号层与 builtin / macro-like 元数据的统一注册层
4. `spio build` 所依赖的 compiler-side source-build 入口
5. 对应的测试、handoff 文档、cross-repo docs gate 和 team runbook

本计划不覆盖：

1. `spio` 自己的 toolchain state / fetch / lockfile / CLI 语义设计
2. 项目级自定义 lexer/parser 规则
3. 完整 package-manager lifecycle
4. 任意私有 `styio` fork 的 source-build 支持

## 3. Principles

1. `binary` 和 `build` 是两条不同 contract，不共享模糊语义。
2. `--machine-info=json` 继续只服务 binary 通道。
3. `--source-build-info=json` 只描述 source-build contract，不替代 machine-info。
4. 官方 source-build 默认源固定为 `https://github.com/eBioRing/Styio.git`。
5. 官方 source-build 通道当前固定：
   - `stable -> stable`
   - `nightly -> nightly`
6. 默认符号层必须有 compiler-owned 单一真相源；IDE、handoff contract、future source-build override 都从这层读取。
7. 本计划优先收敛构建边界和 contract，再扩展优化能力。

## 4. Target Architecture

`styio-nightly` 最终要收敛到如下结构：

```text
styio-nightly
├── binary contract
│   ├── styio --machine-info=json
│   └── styio --compile-plan <path>
├── source-build contract
│   └── styio --source-build-info=json
├── controlled source graph
│   ├── compiler_core
│   ├── std_symbols
│   ├── runtime
│   └── macro_prelude
└── symbol registry
    └── compiler-owned default symbol / builtin / macro metadata
```

### 4.1 Binary Channel

Binary channel 继续发布：

1. `styio --machine-info=json`
2. `styio --compile-plan <path>`
3. receipt / diagnostics / runtime event artifacts
4. `styio-nano` bootstrap/package contract

### 4.2 Build Channel

Build channel 新增并冻结：

1. `styio --source-build-info=json`
2. official source origin / branch mapping
3. controlled component graph
4. supported build mode: `minimal`
5. explicit override points:
   - `source_root`
   - `source_rev`

### 4.3 Controlled Source Graph

当前官方受控子图固定为：

1. `compiler_core`
   - tokenizer / parser / AST / sema / IR / codegen / JIT
2. `std_symbols`
   - 默认符号层、标准类型、builtin 元数据
3. `runtime`
   - runtime support、intrinsic lowering 所需实现
4. `macro_prelude`
   - 默认 macro-like / prelude symbol layer

## 5. Execution Stages

### Stage 1: Contract Split And Registry Unification

目标：

1. 增加 `--source-build-info=json`
2. 保留 `--machine-info=json` 和 `--compile-plan <path>` 的 binary contract 稳定
3. 把默认 builtin / keyword / macro-like 元数据收口到 compiler-owned registry
4. 让 IDE 与 handoff contract 改读同一份 registry
5. 补齐 tests、docs mirror、docs gate、runbook

当前状态：

- **Completed**

已落地证据：

1. `src/StyioParser/SymbolRegistry.*`
2. `src/StyioConfig/SourceBuildInfo.*`
3. `src/StyioIDE/SemDB.cpp` 改读共享 registry
4. `tests/styio_test.cpp` 新增 `--source-build-info=json` 回归
5. `docs/external/for-spio/Styio-Nano-Spio-Coordination.md`
6. `docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md`
7. `scripts/ecosystem-cli-doc-gate.py` 新增 `styio.source_build`

### Stage 2: Source Graph Materialization

目标：

1. 把 `compiler_core / std_symbols / runtime / macro_prelude` 从文档概念落实成稳定 target 图
2. 继续把 `src/main.cpp` 退化成 CLI 装配层
3. 把 source-build 相关实现从 `main.cpp` 拆到独立模块
4. 明确 full compiler 和 `styio-nano` 的 source-build contract 边界，避免不必要的二进制污染

完成标志：

1. controlled component graph 在 CMake target 图中可清晰定位
2. full compiler 的 source-build contract 不再依赖 `main.cpp` 内散落逻辑
3. `styio-nano` 不强制链接 source-build-only contract implementation

当前状态：

- **Baseline completed**

已落地证据：

1. `src/CMakeLists.txt` 现在显式导出：
   - `styio_symbol_core`
   - `styio_runtime_core`
   - `styio_cli_contract_core`
2. `styio` full compiler 链接 `styio_cli_contract_core`
3. `styio-nano` 不再链接 source-build contract implementation

### Stage 3: Official Source-Build Entry

目标：

1. 提供可重复的 compiler-side source-build entrypoint
2. 给定 `source_root + source_rev + minimal`，可稳定构建出可消费 compile-plan 的 `styio`
3. 明确 `source_root` override 满足相同 source-layout contract 时的行为

完成标志：

1. `spio build` 能基于官方源码树构出本地 compiler
2. source-build contract 不要求 `spio` 读取 `styio` 内部实现细节
3. source-build 流程具备最小回归覆盖

当前状态：

- **Baseline completed**

已落地证据：

1. compiler-side helper 已固定为 `scripts/source-build-minimal.sh`
2. `--source-build-info=json` 现在导出 helper script 与 target metadata
3. `tests/styio_test.cpp` 已覆盖 `--source-build-info=json` 与 helper `--help` smoke path

### Stage 4: Minimal Build Semantics

目标：

1. 把 `minimal` 从 CLI/contract 名字落实成真实 compiler-side 语义
2. 支持默认符号层重载、std/runtime/app 跨层内联、激进裁剪的 build-channel 最小闭包
3. 明确哪些 override 是允许的，哪些仍然不开放

完成标志：

1. `minimal` 的编译器行为有明确定义和测试
2. 冗余函数不再无条件进入最终包
3. 默认符号层可作为受控 override surface 工作

当前状态：

- **Baseline completed**

已落地证据：

1. `styio --compile-plan <path>` 现在显式消费 `profile.build_mode`
2. compile-plan `profile.build_mode` 缺失时回落到 `minimal`，显式值当前只允许 `minimal`
3. `receipt.json` 与 compile-plan runtime events 现在回显 `build_mode`
4. `tests/styio_test.cpp` 已覆盖：
   - `minimal` end-to-end receipt/runtime assertions
   - 非法 `profile.build_mode` 的 CLI/diag failure path
5. 默认符号层的官方 override surface 继续固定在 `src/StyioParser/SymbolRegistry.cpp`，由 source-build 通道通过 `source_root`/`source_rev` 控制源码级覆盖

### Stage 5: Hardening And Cutover

目标：

1. 扩大 malformed input / edge compatibility / recovery coverage
2. 固化 source-build docs mirror 和 consumer handoff
3. 把双通道行为纳入 checkpoint-grade delivery floor

完成标志：

1. docs gate、team-docs gate、ecosystem CLI doc gate 都能长期保护 dual-channel contract
2. `spio` 侧 source-build 文档与 `styio` owner 文档无漂移
3. `binary` / `build` 行为都具备明确的 regression evidence

当前状态：

- **Baseline completed**

已落地证据：

1. `scripts/ecosystem-cli-doc-gate.py`、`scripts/team-docs-gate.py` 和 `./scripts/docs-gate.sh` 已把 dual-channel source-build contract 纳入常规门禁
2. `docs/external/for-spio/Styio-Nano-Spio-Coordination.md` 与 `docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md` 已对齐 helper、source origin、channel mapping、build-mode contract
3. `tests/styio_test.cpp` 现在同时覆盖 source-build metadata、helper smoke path、compile-plan `build_mode` 成功路径和失败路径

## 6. Workstreams

### 6.1 Compiler Contract

Owner focus:

1. `--machine-info=json`
2. `--compile-plan <path>`
3. `--source-build-info=json`
4. artifact contract

### 6.2 Source Layout

Owner focus:

1. controlled component graph
2. source-root override compatibility
3. full compiler vs nano separation

### 6.3 Symbol Registry

Owner focus:

1. default symbol layer SSOT
2. IDE / semantic / handoff consumption
3. future source-build override surface

### 6.4 Tests

Owner focus:

1. contract regressions
2. source-build graph regressions
3. IDE symbol-registry consistency

### 6.5 Docs And Governance

Owner focus:

1. handoff docs
2. contract mirror
3. runbook and docs gate

## 7. Acceptance Criteria

本计划完成时，至少要满足：

1. `styio --machine-info=json` 和 `styio --compile-plan <path>` 继续稳定支撑 binary 通道。
2. `styio --source-build-info=json` 稳定描述 build 通道的官方 source-layout contract。
3. 默认符号层只有一份 compiler-owned registry，IDE 和 handoff contract 不再维护第二份表。
4. 官方 source-build 子图可以被稳定定位和构建。
5. `spio build` 能依赖官方 source-build contract，而不是依赖 `styio` 内部私有实现细节。
6. 对 dual-channel contract 的 docs gate、test、runbook 都已接线。

## 8. Related Documents

1. [../external/for-spio/Styio-Nano-Spio-Coordination.md](../external/for-spio/Styio-Nano-Spio-Coordination.md)
2. [Styio-Ecosystem-CLI-Contract-Matrix.md](./Styio-Ecosystem-CLI-Contract-Matrix.md)
3. [../teams/CLI-NANO-RUNBOOK.md](../teams/CLI-NANO-RUNBOOK.md)
4. [../teams/FRONTEND-RUNBOOK.md](../teams/FRONTEND-RUNBOOK.md)
5. [../teams/IDE-LSP-RUNBOOK.md](../teams/IDE-LSP-RUNBOOK.md)
6. [../teams/TEST-QUALITY-RUNBOOK.md](../teams/TEST-QUALITY-RUNBOOK.md)
7. [../teams/DOCS-ECOSYSTEM-RUNBOOK.md](../teams/DOCS-ECOSYSTEM-RUNBOOK.md)

## 验收条件

1. `styio --machine-info=json`, `styio --compile-plan <path>`, and `styio --source-build-info=json` remain aligned with the consumer handoff docs.
2. Source-build helper, source graph, symbol registry, tests, and docs gates name the same compiler-side contract.
3. Any remaining ecosystem hardening is either moved into the master plan or routed through the common foundation plan before upper-layer work depends on it.
4. `python3 scripts/ecosystem-cli-doc-gate.py`, `python3 scripts/docs-audit.py`, and the relevant CLI / Nano tests pass before this plan is treated as current evidence.
