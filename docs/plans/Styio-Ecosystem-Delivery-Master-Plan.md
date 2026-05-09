# Styio Ecosystem Delivery Master Plan

**Purpose:** 作为 `styio-nightly`、`styio-spio`、`styio-view` 的统一交付总纲，定义权威里程碑、跨仓文档落地顺序、共享门禁与完成定义，确保三仓以同一套语言产品目标推进，而不是各自演化。当前它仍然是 active ecosystem plan，而不是已闭合的历史记录。

**Last updated:** 2026-04-21

**Plan status:** Active ecosystem master plan. `styio-nightly` 已完成 compiler-side dual-channel/source-build baseline 和统一 docs gate 基线，但本总纲对应的跨仓交付目标仍停留在未闭合状态，尤其是 `M4`-`M6` 的 runtime/module/mobile/cloud/hosted product closure。

## 0. Current Closure Snapshot

This file stays active because repo-local closure is no longer the only question. The current high-signal state is:

1. `styio-nightly`
   - Compiler-side binary/build contract baseline is complete.
   - Source-build helper, symbol registry SSOT, and docs gates are live.
   - Remaining work here is mostly ecosystem-facing hardening, milestone debt closure, and cross-repo coordination.
2. `styio-spio`
   - Package-manager and cloud-control semantics have active baselines, but live compile workflow, cloud control plane, registry hardening, and hosted execution remain open.
3. `styio-view`
   - Product shell and platform/toolchain plans are still active; mobile/cloud/hosted product closure is not done.

Treat this plan as the answer to "is the ecosystem done?", not "is one repo locally green?".

## 1. 产品目标与完成定义

这份总纲完成时，Styio 必须已经具备一个编程语言产品的完整能力面，而不是只闭合单一主线：

1. 语言、编译器、JIT、runtime、IDE/LSP handoff 已稳定发布。
2. 包管理、resolver、cache、vendor、build/run/test、pack/publish、toolchain lifecycle 已闭合。
3. IDE 可稳定消费 project graph、execution、runtime、toolchain、registry、deploy preflight。
4. AI 面板、主题系统、模块运行时、移动端、云执行和 hosted workspace 已纳入同一产品语义。
5. 所有关键失败路径都返回 machine-readable diagnostics，而不是只靠 stderr 文本。

这份总纲不是“做完所有历史 backlog”，而是把三仓收敛到一个统一语言平台的完成态。

## 2. 权威、镜像与文档传播

`styio-nightly/docs/plans/Styio-Ecosystem-Delivery-Master-Plan.md` 是唯一权威副本。

镜像与关联文档的职责固定如下：

1. `styio-spio/docs/planning/Styio-Ecosystem-Delivery-Master-Plan.md`
   - 只保留 `spio` 侧里程碑映射、repo exit 和本仓 gate 解释。
2. `styio-view/docs/plans/Styio-Ecosystem-Delivery-Master-Plan.md`
   - 只保留 `view` 侧里程碑映射、repo exit 和本仓 gate 解释。
3. `styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md`
   - 继续作为 CLI/machine contract 的唯一冻结矩阵。
4. `docs/external/for-*`、`docs/external/for-styio/`、`docs/contracts/`
   - 只承载消费者 handoff 和局部合同，不得偷改 milestone 定义。

任何一个 checkpoint 只要改了里程碑定义、shared contract、repo exit、handoff 路径或 cutover gate，必须按这个顺序同步：

1. 更新本文件。
2. 更新 `spio/view` 的镜像总纲。
3. 更新受影响的本仓计划映射：
   - `styio-spio/docs/planning/Spio-Master-Plan.md`
   - `styio-view/docs/plans/Styio-View-Implementation-Plan.md`
4. 更新消费者 handoff 文档与 team runbook。
5. 更新测试目录、验证矩阵和 `docs/history/YYYY-MM-DD.md`。

## 3. 统一治理与角色

### 3.1 Program Coordinator

Program Coordinator 负责：

1. 里程碑顺序和 checkpoint 命名
2. 三仓文档传播和镜像一致性
3. 跨仓 gate 定义
4. handoff 审批
5. 冲突升级和 cutover 判定

### 3.2 Repo Leads

每个仓库保持一个 lead：

1. `styio-nightly`
   - 语言语义、编译器 contract、runtime、IDE/LSP、发布真相。
2. `styio-spio`
   - project workflow、resolver/cache、toolchain lifecycle、registry/package、compat/security。
3. `styio-view`
   - editor shell、adapter/contracts、runtime/agent、environment UX、module/platform、theme/UX。

### 3.3 Repo Sub-Agents

Repo lead 可以继续下拆 sub-agent，但以下 choke point 不允许并行改写：

1. `machine-info` / `compile-plan` / `runtime events` 的 published shape
2. `project_graph` / `toolchain_state` / `workflow_success_payloads` 的 published shape
3. `ExecutionSession` / `RuntimeEventEnvelope` / environment/deploy contract
4. 跨仓里程碑 ID、repo exit 和 cutover gate

## 4. 现有计划映射

| 统一里程碑 | `styio-nightly` | `styio-spio` | `styio-view` |
|------------|-----------------|--------------|--------------|
| `M0` 程序治理锁定 | ecosystem total plan + CLI matrix + docs gates | docs maintenance + verification matrix + mirror plan | documentation policy + implementation-plan mapping + mirror plan |
| `M1` 编译器合同闭环 | compiler/runtime/IDE handoff 主线 | compat + compile-plan round-trip | language/execution contract 消费 |
| `M2` 包管理与环境闭环 | published compiler truth 稳定 | phases 2-6 主线闭合 | project graph / toolchain / registry 真实消费 |
| `M3` IDE 核心闭环 | IDE/LSP/runtime published payload | workflow/toolchain/deploy payload 稳定 | W3-W5 + execution/environment shell 闭合 |
| `M4` runtime/AI/theme/module 闭环 | runtime events、IDE runtime、module/runtime hooks | module/distribution/agent-support payload | W6-W8-W10 闭合 |
| `M5` 移动端/云执行/hosted 闭环 | remote/cloud compiler contract | hosted/toolchain/distribution support | W9 闭合 |
| `M6` 三仓统一发布 | hardening + release truth | split-ready + release-grade package manager | product-complete IDE + cross-platform closure |

## 5. 共享 contract 面

### 5.1 `styio-nightly`

`styio-nightly` 负责发布：

1. `styio --machine-info=json`
2. `styio --compile-plan <path>`
3. diagnostics / receipt / artifact layout
4. runtime event stream version
5. compile/run session result
6. IDE/LSP machine-facing payload

### 5.2 `styio-spio`

`styio-spio` 负责发布：

1. manifest / lockfile schema
2. workflow success / failure payload
3. `project_graph`
4. `toolchain_state`
5. `source_state`
6. registry / package / deploy preflight payload
7. project-local and hosted environment lifecycle

### 5.3 `styio-view`

`styio-view` 负责发布：

1. `ExecutionSession`
2. `RuntimeEventEnvelope`
3. project graph consumption contract
4. environment/deploy shell behavior
5. blocked / partial / live adapter semantics
6. AI / theme / module / platform capability surface

## 6. 里程碑

### M0 程序治理锁定

能力出口：

1. 三仓采用同一套 milestone ID、checkpoint 命名和完成定义。
2. 权威总纲、镜像总纲、CLI matrix、repo-local 计划映射全部接线。
3. team runbook、history、handoff 和 test catalog 都能指回同一条计划主线。

文档落点：

1. 本文件
2. `styio-spio/docs/planning/Styio-Ecosystem-Delivery-Master-Plan.md`
3. `styio-view/docs/plans/Styio-Ecosystem-Delivery-Master-Plan.md`
4. 三仓 `docs/teams/COORDINATION-RUNBOOK.md`
5. `styio-spio/docs/governance/Docs-Maintenance-Model.md`
6. `styio-view/docs/specs/DOCUMENTATION-POLICY.md`

门禁出口：

1. `python3 styio-nightly/scripts/docs-audit.py`
2. `python3 styio-nightly/scripts/team-docs-gate.py`
3. `python3 styio-nightly/scripts/ecosystem-cli-doc-gate.py --require-workspace --json`
4. `python3 styio-spio/scripts/repo-hygiene-check.py --repo-root . --mode tracked`
5. `python3 styio-spio/scripts/submit-gate.py --profile pre-push`
6. `python3 styio-view/scripts/check_repo_hygiene.py`

### M1 编译器合同闭环

能力出口：

1. `machine-info`、`compile-plan`、diagnostics、receipt、artifact 行为完全冻结。
2. `runtime events`、IDE/LSP handoff 有正式版本边界。
3. `spio` 和 `view` 不再猜测 `nightly` 未发布能力。

文档落点：

1. `styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md`
2. `styio-nightly/docs/external/for-spio/`
3. `styio-nightly/docs/external/for-ide/`
4. `styio-spio/docs/external/for-styio/Styio-External-Interface-Requirement-Spec.md`
5. `styio-view/docs/external/for-styio/`

门禁出口：

1. `styio_contract_compat_gate`
2. `styio_compile_plan_contract_gate`
3. `styio-nightly` milestone / five-layer / security / IDE/LSP 相关 gate
4. `view` adapter fixture 与 handoff contract tests

### M2 包管理、环境与注册表闭环

能力出口：

1. `spio` 的 manifest/lock、resolver/cache、fetch/vendor、build/run/test、pack/publish、tool install/use/pin/switch、registry client/server 全部形成 live workflow。
2. `project_graph`、`toolchain_state`、`source_state`、deploy preflight 变成正式 payload。
3. `view` 不再使用私有文件推断 package/toolchain/registry 状态。

文档落点：

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-spio/docs/governance/Spio-Entry-Argument-Index.md`
3. `styio-spio/docs/operations/Spio-Verification-Matrix.md`
4. `styio-view/docs/external/for-spio/`
5. `styio-view/docs/contracts/`

门禁出口：

1. `spio_cli_gate`
2. `spio_manifest_lock_gate`
3. `spio_workflow_gate`
4. `spio_registry_server_gate`
5. `spio_submit_gate`
6. `styio-spio/scripts/styio-interface-gate.py --require-compile-plan --json`

### M3 IDE 核心闭环

能力出口：

1. editor shell、project graph、execution routing、diagnostics、environment/deploy shell 形成统一 IDE 主线。
2. scratch、project、workspace、多包路径统一到同一 `ExecutionSession`。
3. IDE 内可完成环境安装、切版本、JIT、build/test、deploy preflight。

文档落点：

1. `styio-view/docs/plans/Styio-View-Implementation-Plan.md`
2. `styio-view/docs/contracts/`
3. `styio-view/docs/external/for-styio/`
4. `styio-view/docs/external/for-spio/`
5. `styio-view/docs/assets/workflow/TEST-CATALOG.md`

门禁出口：

1. `cd frontend/styio_view_app && flutter analyze && flutter test`
2. `cd prototype && npm run selftest:editor` 在手写 Web 主线受影响时必跑
3. `adapter / schema / handoff` 合同 fixtures 全绿

### M4 Runtime / AI / Theme / Module 闭环

能力出口：

1. `runtime_events v1` 真正驱动 runtime surface、thread lanes、execution graph 和 debug console。
2. AI provider adapter、prompt/profile、agent panel 形成正式产品面。
3. 主题系统、模块 manifest、capability matrix、staged update 和卸载回收闭环。

文档落点：

1. `styio-nightly` runtime / IDE 文档与 handoff
2. `styio-spio` module/distribution/agent-support 合同
3. `styio-view` 的 runtime/agent、theme、module/platform 计划与合同
4. 受影响 ADR、TEST-CATALOG、teams runbook

门禁出口：

1. `nightly` runtime/IDE 相关 gate
2. `spio` distribution/registry/toolchain 相关 gate
3. `view` runtime/agent/module/theme tests

### M5 移动端、云执行与 Hosted Workspace 闭环

能力出口：

1. Android 本地优先、iOS 云执行、Web hosted workspace 全部纳入正式 platform matrix。
2. 本地不可用能力必须 machine-readable 地返回 `blocked`，不能由 UI 猜测。
3. 云执行与 hosted workspace 仍共享同一 project/toolchain/runtime 模型。

文档落点：

1. `styio-view` 平台执行策略、W9 文档与 hosted workspace 生命周期
2. `styio-spio` hosted/toolchain/distribution 支持合同
3. `styio-nightly` remote/cloud compile contract 与 machine-info 说明

门禁出口：

1. 平台矩阵与 hosted workflow tests
2. `view` 的移动端/云端 capability tests
3. `spio` 的 hosted/distribution payload fixtures
4. `python3 styio-spio/scripts/ecosystem-product-gate.py --styio-bin /absolute/path/to/styio --spio-bin /absolute/path/to/spio --json`

### M6 三仓统一发布完成态

能力出口：

1. CLI、IDE、AI、runtime、module、mobile、cloud、hosted 路径全部形成统一语言产品。
2. 三仓可被共同维护，而不是依赖临时人工对齐。
3. sample workspace matrix 可以稳定复现所有关键成功和失败路径。

门禁出口：

1. 三仓现有 docs/quality/delivery gate 全绿
2. canonical sample workspace matrix 全绿
   当前 baseline gate 为 `python3 styio-spio/scripts/ecosystem-sample-workflow-gate.py --styio-bin /absolute/path/to/styio --spio-bin /absolute/path/to/spio --json`
   当前 baseline 至少覆盖 managed toolchain switch、vendored offline、registry-hosted source、以及多包 workspace 下 `run/test/publish` 的显式 `--package` 选择与歧义保护
3. hosted product gate 全绿
   当前权威 gate 为 `python3 styio-spio/scripts/ecosystem-product-gate.py --styio-bin /absolute/path/to/styio --spio-bin /absolute/path/to/spio --json`
   当前已覆盖 desktop local CLI-owned IDE workflow、desktop local vendored/offline 恢复路径（`fetch -> vendor -> clear SPIO_HOME -> fetch --offline -> reinstall/use/pin -> run`）、desktop local 多包 workspace 路由与 publish 歧义保护、desktop local filesystem registry 分发闭环（重开已 pin 的 library workspace 并先完成真实 build，再执行成功 publish/consume、republish conflict、坏 registry consumer 的结构化 `fetch` 失败）、desktop local compile/dependency/deployment 失败矩阵、hosted control plane、`styio-view` 的 `install/use/pin/fetch/vendor/pack/run/test/preflight` 产品 workflow、managed-toolchain switch-and-return、多包 workspace 下的 package 路由与 publish 歧义保护、hosted filesystem registry 分发闭环（先 build hosted library package，再执行成功 publish/consume、republish conflict、坏 registry consumer 的结构化 `fetch` 失败），以及 `iOS cloud-only` / `Web hosted-only` / `Android cloud fallback`
   当前还明确要求 desktop local 和 hosted 两条路线上，成功的 `pack` 与 `publish --dry-run` 都必须保留真实 `archive_path` 并能在 gate 中验证产物存在
   当前还明确覆盖六条高价值失败路径：desktop local 和 hosted 两条路线上，编译失败都必须回传 machine-readable diagnostics 与 `compile.failed` runtime events；依赖获取失败都必须回传结构化 dependency error payload；publish preflight 失败都必须回传结构化 deployment error payload
4. `ecosystem-cli-doc-gate.py --require-workspace --json` 全绿
5. 任一失败路径都能回到 machine-readable diagnostics

## 7. 当前优先顺序

当前交付顺序仍保持：

1. 编译器和 machine contracts
2. 包管理 live workflow
3. IDE execution/environment/runtime shell
4. AI/theme/module/platform 深化
5. 视觉和交互打磨最后收口

除非有书面 checkpoint 改写，这个顺序不得反转。

## 8. 统一 sample matrix

所有 milestone 最终都要在同一套 canonical sample workspaces 上证明：

1. 单包 `bin` 项目
2. `lib + test` 项目
3. 多包 workspace
4. fetch/vendor/offline
5. toolchain install/use/pin/switch
6. JIT run / build / test
7. publish / deploy preflight
8. runtime events / diagnostics
9. Android / iOS cloud / hosted workspace / blocked path
