# Styio / Pafio Coordination Plan

**Purpose:** 从 `styio` 主仓库视角，冻结 `pafio` 双通道消费 `styio` 的 handoff contract：`binary` 通道继续消费已发布的 `styio` 二进制和 compile-plan 合同，`build` 通道消费官方受控源码子图和 source-build 元数据；同时保留 `styio-nano` bootstrap/package contract 作为 binary 通道的一部分。本文件描述的是 `styio` 侧 handoff contract，不替代 `styio-pafio` 仓库内自己的 SSOT。

**Last updated:** 2026-04-20

**Related docs:**
- [`Styio Handle, Capability, and Failure Type System`](../../design/Styio-Handle-Capability-Type-System.md)

---

## 1. 目标

当前阶段要解决的是一件很具体的事：

`styio` 需要同时支持两条由 `pafio` 触发的消费路径：

- `binary`：继续用已发布 `styio` 二进制、`--machine-info=json`、`--compile-plan` 和 `styio-nano` bootstrap/package contract 支撑开箱即用流程。
- `build`：通过官方源码树和稳定的 source-build 元数据，让 `pafio build` 能够拉取 `styio` 源码、按受控子图分层构建 compiler/std-symbols/runtime，并在项目级执行最小化构建。

也就是说，`styio-nano` 仍然要能自己完成最小闭环，用于 bootstrap、回归测试与 edge profile 验证；但长期来看，远端仓库消费、安装、缓存、pin、vendor、项目工具链切换，不应继续堆在 `styio` 主编译器里，而应由 `styio-pafio` 接管。

因此，本阶段的边界是：

- `styio` 负责：
  - 生成 `styio-nano` 包
  - 生成本地 source closure
  - 本地重建 `styio-nano`
  - 将包写入静态仓库布局
  - 从静态仓库布局读取包
- `styio-pafio` 后续负责：
  - 远端 registry / package source 的生命周期管理
  - 下载、缓存、校验、pin、vendor
  - 项目级 toolchain/use/install 流程
  - 面向用户的 package UX

---

## 2. 双通道 handoff contract

### 2.1 `binary` 通道

`pafio use binary` 继续消费已发布的 full `styio` compiler。`styio` 侧当前的公开合同仍然是：

- `styio --machine-info=json`
- `styio --compile-plan <path>`
- `styio-nano` bootstrap/package contract

这条通道仍然是黑盒消费：`pafio` 不需要理解 `styio` 内部源码图，只根据 machine-info、compat matrix 和 compile-plan handoff 决定能否继续。

### 2.2 `build` 通道

`pafio use build` 消费 `styio` 的官方源码图，而不是已发布二进制。当前固定的官方 source-build 入口是：

- `styio --source-build-info=json`

当前 source-build contract 冻结这些字段：

- 官方源码源：`https://github.com/SymPolicy/Styio.git`
- 通道分支：
  - `stable -> stable`
  - `nightly -> nightly`
- 受控子图：
  - `compiler_core`
  - `std_symbols`
  - `runtime`
  - `macro_prelude`
- 官方 build mode：
  - `minimal`
- 官方 helper 入口：
  - `scripts/source-build-minimal.sh`
- compile-plan `profile.build_mode`：
  - 缺失时按 `minimal` 处理，兼容旧的 v1 plan
  - 当前只接受显式值 `minimal`
  - 其它值必须由 compiler-side compile-plan consumer 直接拒绝
- 允许的 override：
  - `source_root`
  - `source_rev`

`source-build-info` 的职责不是替代 `machine-info`，而是描述 `pafio build` 可以依赖的官方源码布局与 override 边界。

当前 helper 约定是：

- `scripts/source-build-minimal.sh`
  - 使用当前仓库根目录作为 `-S`
  - 默认输出到 `build/source-minimal/`
  - 只构建 full `styio` compiler
  - 显式关闭 `STYIO_BUILD_NANO`

### 2.3 默认符号层 contract

`styio` 当前把默认符号层收敛到：

- `src/StyioParser/SymbolRegistry.hpp`
- `src/StyioParser/SymbolRegistry.cpp`

这层是当前 source-build override surface 的唯一真相源，用来描述：

- 默认符号
- 内建类型
- macro-like 符号
- prelude / builtin member surface

IDE、handoff contract 和未来的 source-build 优化逻辑都必须从这个 registry 读取默认符号层，不得再维护第二份散落的 builtin/keyword 表。

---

## 3. `styio` 当前已经落地的能力

### 3.1 本地导出

`styio --nano-create --nano-mode=local-subset ...`

该路径当前会：

1. 读取 nano profile
2. 生成 `styio_nano_profile.cmake`
3. 收集 source closure
4. 将 closure 拷贝到 materialized package 目录
5. 生成 package-local `CMakeLists.txt`
6. 在包内直接重建 `bin/styio-nano`
7. 写出 receipt

materialized package 目录至少包含：

- `bin/styio-nano`
- `styio-nano.profile.toml`
- `styio_nano_profile.cmake`
- `styio-nano-package.toml`
- `source-closure-manifest.txt`
- `build-styio-nano.sh`
- `src/...` closure files

### 3.2 本地 publish

`styio --nano-publish ...`

该路径当前只支持将**已经 materialize 完成**的 nano 包写入**本地静态仓库**。它不负责远端鉴权、推送协议、缓存策略，也不承担 package manager 的角色。

### 3.3 静态仓库 consume

`styio --nano-create --nano-mode=cloud ...`

该路径当前支持：

- 本地目录
- `file://`
- `http://`
- `https://`

但这条路径的定位是 bootstrap / regression / contract validation，不是长期 package UX。

### 3.4 machine-info 能力声明

当前 full `styio` 的 `--machine-info=json` 已声明这些 contract capability：

- `nano_package_materialize`
- `nano_package_local_subset_closure_v1`
- `nano_package_registry_consume_v1`
- `nano_package_registry_publish_v1`

`pafio` 后续可以用这些 capability 做兼容性判定，而不是猜测某个 `styio` 二进制支不支持 nano package workflow。

---

## 4. 当前静态仓库 contract

### 4.1 marker

仓库根必须包含：

`styio-nano-repository.json`

当前 schema：

```json
{
  "kind": "styio-nano-static",
  "schema_version": 1
}
```

### 4.2 index entry

索引条目路径：

`index/<package>/<version>.json`

当前字段：

```json
{
  "schema_version": 1,
  "package": "edge/default",
  "version": "0.0.1",
  "channel": "nano",
  "sha256": "<64-hex>",
  "size_bytes": 12345,
  "blob_path": "blobs/sha256/ab/cd/<sha>.tar",
  "published_at": "2026-04-12T00:00:00Z"
}
```

### 4.3 blob layout

当前 blob 路径：

`blobs/sha256/<sha[0:2]>/<sha[2:4]>/<sha>.tar`

blob 内容是一个 tar 包，解开后必须能解析出 package root，并且 package root 中必须存在：

- `bin/styio-nano`

如果有 profile，也可附带：

- `styio-nano.profile.toml`
- `styio-nano-package.toml`

### 4.4 校验要求

`styio` 侧当前会做：

- marker JSON 校验
- index entry JSON 校验
- blob `size_bytes` 校验
- blob `sha256` 校验
- 解包后 package root 结构校验

这是 `pafio` 后续应保持兼容的最小校验集。

---

## 5. package receipt contract

当前 `styio-nano-package.toml` 至少使用 `[package]` 段；`styio` publish 侧当前会从 receipt 中读取这些字段作为默认值：

- `name`
- `version`
- `channel`
- `binary`
- `profile`

对 publish 来说，当前默认规则是：

- `package` 未显式指定时：
  - 先取 receipt `name`
  - 再退回 package dir basename
- `version` 未显式指定时：
  - 取 receipt `version`
- `channel` 未显式指定时：
  - 取 receipt `channel`
  - 再退回 `nano`

因此，`local-subset` 包现在会主动写出：

- `version = <styio compiler version>`
- `channel = "nano"`

这样 publish 不需要每次都额外传 version。

---

## 6. `styio` 与 `pafio` 的职责切分

### 6.1 `styio` 应继续保留的职责

这些职责留在 `styio` 内是合理的：

- 从 profile 裁剪出 source closure
- 生成 package-local build helper
- 用 closure 重建 `styio-nano`
- 对静态仓库 contract 做 producer / consumer 验证
- 对 nano package workflow 做回归测试
- 维护官方 source-build 子图与 `--source-build-info=json`
- 维护默认符号层 registry 与 source-build 最小 override surface

原因很简单：这些都直接依赖编译器内部知识，尤其是：

- 哪些源码必须进入 closure
- 哪些 profile 开关映射到哪些 compile definitions
- 哪些 runtime / CLI 能力必须被裁剪

### 6.2 应由 `pafio` 接管的职责

这些职责不应长期继续堆在 `styio` 主编译器里：

- registry source 管理
- 远端下载/重试/镜像
- 本地缓存目录布局
- pin / install / use / vendor
- project-level toolchain selection
- build 通道源码拉取、通道分支选择、revision 锁定
- source-build toolchain state / lockfile / local checkout policy
- package search / registry UX
- 发布流程中的权限与分发策略

换句话说：

- `styio` 更像 nano package 的**producer + verifier**
- `pafio` 应成为 nano package 的**distributor + installer + lifecycle manager**

---

## 7. `pafio` 后续需要使用的 `styio` 接口

### 7.1 已可用接口

`pafio` 现在就可以依赖这些 `styio` 能力：

1. `--machine-info=json`
2. `--nano-create` 本地导出
3. `--nano-create` 静态仓库读取
4. `--nano-publish` 本地静态仓库写入

### 7.2 建议使用方式

建议 `pafio` 后续按这个模型调用：

1. 若本地需要生成 nano 包：
   - 调用 `styio --nano-create --nano-mode=local-subset ...`
2. 若需要写入本地 staging repo：
   - 调用 `styio --nano-publish ...`
3. 若需要做 contract self-check：
   - 调用 `styio --nano-create --nano-mode=cloud ...`

这里的重点是：

- `pafio` 不需要复制 source closure 逻辑
- `pafio` 不需要复制 package-local CMake 生成逻辑
- `pafio` 只需要编排 `styio` 已提供的 producer / verifier 接口

### 7.3 `styio` 侧交接完成清单

这个仓库对 `pafio` 的“已完成工作”不应理解成“完整包管理系统已经完成”，而应理解成：

- `styio` 是否已经把 **producer / verifier contract** 交付完整
- `pafio` 是否已经能在**不复制编译器内部逻辑**的前提下消费这些 contract

当前 `styio` 侧 checklist 如下：

| Checklist item | Owner | Status in `styio` | Evidence |
|----------------|-------|-------------------|----------|
| `--machine-info=json` 暴露稳定 handshake 与 nano package capability | `styio` | Completed | `src/main.cpp`, `StyioDiagnostics.MachineInfoJsonReportsStableHandshakeFields`, `StyioNano.MachineInfoReflectsPrunedProfile` |
| `--nano-create --nano-mode=local-subset` 能 materialize nano 包并写出 receipt / closure / build helper | `styio` | Completed | `StyioNanoPackage.LocalSubsetConfigMaterializesBundle`, `StyioNanoPackage.LocalSubsetCliMaterializesBundle` |
| materialized package 能在包内重建 `bin/styio-nano` | `styio` | Completed | `StyioNanoPackage.LocalSubsetConfigMaterializesBundle` |
| `--nano-publish` 能写入静态仓库 marker / index / blob | `styio` | Completed | `StyioNanoPackage.PublishConfigWritesRepositoryAndRoundTripsToCloudInstall` |
| `--nano-create --nano-mode=cloud` 能从静态仓库 consume 并安装 nano 包 | `styio` | Completed | `StyioNanoPackage.CloudRepositoryConfigMaterializesBundle`, `StyioNanoPackage.PublishConfigWritesRepositoryAndRoundTripsToCloudInstall` |
| 静态仓库 consume 路径会执行 marker / index / sha256 / size / package-root 最小校验 | `styio` | Completed | 本文 §4.4 contract；consume roundtrip 覆盖见 `StyioNanoPackage.CloudRepositoryConfigMaterializesBundle` |
| receipt 默认字段可驱动 publish 默认 package/version/channel 解析 | `styio` | Completed | 本文 §5 contract；publish roundtrip 覆盖见 `StyioNanoPackage.PublishConfigWritesRepositoryAndRoundTripsToCloudInstall` |
| `pafio` 可只编排现有 producer / verifier 接口，而无需复制 closure/CMake 逻辑 | `styio` contract for `pafio` | Completed | 本文 §7.1-§7.2；接口入口为 `--machine-info=json` / `--nano-create` / `--nano-publish` |
| 远端 registry source、cache、pin、vendor、install/use/search UX | `pafio` | Out of scope for this repo | `docs/specs/REPOSITORY-MAP.md`, 本文 §6.2 |
| 完整 registry service protocol（channel index、latest alias、listing API、auth/signing/trust） | `pafio` | Out of scope for this repo | 本文 §8.1 |
| `pafio build/check/run/test` 所需 compile-plan live handoff | `styio` | Completed baseline | 本文 §8.2；`StyioDiagnostics.CompilePlan*` |
| 比 source closure 更细粒度的最小闭包优化 | `styio` | Pending | 本文 §8.3 |

因此，这个仓库目前已经完成的是：

- `pafio` 所需的 **static nano package contract**
- `pafio` 可调用的 **producer / verifier CLI surface**

这个仓库尚未完成、也不应该在这里伪装完成的是：

- `pafio` 自身的完整 package-manager lifecycle
- 远端 registry/service 语义
- 函数级或更细粒度的 closure 极限裁剪

---

## 8. 仍然缺少、需要后续联动的部分

这些仍然是后续阶段的工作，不应伪装成“已经完成”：

### 8.1 真正的仓库索引服务语义

当前 contract 仍是“静态仓库布局”，不是完整的 registry service protocol。

也就是说，目前有：

- marker
- index file
- blob layout

但还没有：

- channel index
- latest alias
- package listing API
- delta/update protocol
- auth / signing / trust model

这些更适合在 `pafio` 仓库侧定义。

### 8.2 compile-plan live handoff

这部分现在已经进入 **baseline live** 状态：

- `--machine-info=json` 会公开 `supported_contracts.compile_plan:[1]`
- `styio --compile-plan <path>` 会消费 versioned plan，并支持 `build` / `check` / `run` / `test`
- compile-plan 执行会在约定的 `outputs.build_root` / `artifact_dir` / `diag_dir` 内写出 receipt、编译产物和 `diagnostics.jsonl`
- 若 plan 非法或 `--compile-plan` 与其它入口参数冲突，只要能解析到 `outputs.diag_dir`，也会把 `STYIO_CLI` 诊断写入该目录；stderr 同样保持 machine-readable JSONL

因此，`pafio build/check/run/test` 已经可以真正走 live path，而不需要复制编译器内部逻辑。

但这部分与 nano package workflow 相关，却不是同一个 contract，不能混在一起。

当前仍需继续收紧的是：

- 更宽的 compatibility edge / malformed-input 矩阵覆盖
  当前已覆盖的负路径包括 invalid JSON、invalid intent、CLI conflict、generated_by mismatch、unsupported target kind、absolute-path guard 和 missing-outputs stderr fallback
- contract 字段稳定性与文档同步
- 在不扩张 package-manager scope 的前提下持续维护 producer boundary

### 8.3 更细粒度的 closure 剪裁

当前 local-subset 已经做到 source closure + package-local rebuild，但仍不是“函数级最小闭包”。

因此对 edge device 而言，当前是：

- 已进入可用阶段
- 但仍不是终极最小体积

这属于 `styio` 侧后续继续做的优化项，而不是 `pafio` 侧任务。

---

## 9. 推荐的阶段性交接顺序

### Phase A: `styio` 内自举稳定

目标：

- local-subset 可稳定产包
- publish / consume roundtrip 稳定
- machine-info capability 稳定

当前状态：已基本完成。

更精确地说：按 §7.3 的 `styio` 侧 checklist，**static nano contract 与 compile-plan live handoff baseline 已完成**；后续项集中在更细粒度 closure 收缩与 compile-plan edge hardening，而不是 package-manager UX 本身。

### Phase B: `pafio` 接管本地 lifecycle

目标：

- `pafio` 调用 `styio` 生成 nano 包
- `pafio` 调用 `styio` 发布到本地 staging repo
- `pafio` 调用 `styio` 做 install 验证

### Phase C: `pafio` 接管远端分发

目标：

- registry source abstraction
- cache / pin / vendor
- install/use UX
- remote package transport

### Phase D: `styio` 继续收缩 nano closure

目标：

- 更小的 closure
- 更少的链接噪音
- 更小的 edge binary footprint

---

## 10. 本阶段结论

到当前为止，`styio` 侧已经具备：

- 生成 `styio-nano` 包
- 以 source closure 形式重建 `styio-nano`
- 写入静态仓库
- 从静态仓库读取并安装
- 用测试验证 publish -> consume roundtrip

因此，对 `pafio` 的下一步配合方式应当是：

- 不复制 `styio` 的 nano package producer 逻辑
- 直接消费当前静态仓库 contract
- 在 `pafio` 仓库内定义更高层的远端 lifecycle 与 UX

如果问题是“本仓库是否已经完成了它该完成的那部分 package-manager handoff 工作”，答案是：

- 对 **static nano package contract** 而言：**已完成**
- 对 **完整 package-manager system** 而言：**不属于本仓库完成条件**
- 对 **compile-plan live handoff baseline** 而言：**已完成**
- 对 **更细粒度 closure / compile-plan edge hardening** 而言：**仍待后续补完**

这就是当前阶段的 handoff 边界。
