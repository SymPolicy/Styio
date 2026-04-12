# Styio Nano / Spio Coordination Plan

**Purpose:** 从 `styio` 主仓库视角，冻结当前 `styio-nano` 的包产出、静态仓库布局、bootstrap 用法与后续交接边界，供 `styio-spio` 后续接管远端分发、安装、缓存与项目级生命周期时使用；本文件描述的是 `styio` 侧 handoff contract，不替代 `styio-spio` 仓库内自己的 SSOT。

**Last updated:** 2026-04-12

**Related docs:**
- [`Styio Ecosystem Bootstrap Plan`](../plans/Styio-Ecosystem-Bootstrap-Plan.md)
- [`Styio Handle, Capability, and Failure Type System`](../design/Styio-Handle-Capability-Type-System.md)

---

## 1. 目标

当前阶段要解决的是一件很具体的事：

`styio` 需要能够自己完成 `styio-nano` 的最小闭环，用于 bootstrap、回归测试与本地 edge profile 验证；但长期来看，远端仓库消费、安装、缓存、pin、vendor、项目工具链切换，不应继续堆在 `styio` 主编译器里，而应由 `styio-spio` 接管。

因此，本阶段的边界是：

- `styio` 负责：
  - 生成 `styio-nano` 包
  - 生成本地 source closure
  - 本地重建 `styio-nano`
  - 将包写入静态仓库布局
  - 从静态仓库布局读取包
- `styio-spio` 后续负责：
  - 远端 registry / package source 的生命周期管理
  - 下载、缓存、校验、pin、vendor
  - 项目级 toolchain/use/install 流程
  - 面向用户的 package UX

---

## 2. `styio` 当前已经落地的能力

### 2.1 本地导出

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

### 2.2 本地 publish

`styio --nano-publish ...`

该路径当前只支持将**已经 materialize 完成**的 nano 包写入**本地静态仓库**。它不负责远端鉴权、推送协议、缓存策略，也不承担 package manager 的角色。

### 2.3 静态仓库 consume

`styio --nano-create --nano-mode=cloud ...`

该路径当前支持：

- 本地目录
- `file://`
- `http://`
- `https://`

但这条路径的定位是 bootstrap / regression / contract validation，不是长期 package UX。

### 2.4 machine-info 能力声明

当前 full `styio` 的 `--machine-info=json` 已声明这些 contract capability：

- `nano_package_materialize`
- `nano_package_local_subset_closure_v1`
- `nano_package_registry_consume_v1`
- `nano_package_registry_publish_v1`

`spio` 后续可以用这些 capability 做兼容性判定，而不是猜测某个 `styio` 二进制支不支持 nano package workflow。

---

## 3. 当前静态仓库 contract

### 3.1 marker

仓库根必须包含：

`styio-nano-repository.json`

当前 schema：

```json
{
  "kind": "styio-nano-static",
  "schema_version": 1
}
```

### 3.2 index entry

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

### 3.3 blob layout

当前 blob 路径：

`blobs/sha256/<sha[0:2]>/<sha[2:4]>/<sha>.tar`

blob 内容是一个 tar 包，解开后必须能解析出 package root，并且 package root 中必须存在：

- `bin/styio-nano`

如果有 profile，也可附带：

- `styio-nano.profile.toml`
- `styio-nano-package.toml`

### 3.4 校验要求

`styio` 侧当前会做：

- marker JSON 校验
- index entry JSON 校验
- blob `size_bytes` 校验
- blob `sha256` 校验
- 解包后 package root 结构校验

这是 `spio` 后续应保持兼容的最小校验集。

---

## 4. package receipt contract

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

## 5. `styio` 与 `spio` 的职责切分

### 5.1 `styio` 应继续保留的职责

这些职责留在 `styio` 内是合理的：

- 从 profile 裁剪出 source closure
- 生成 package-local build helper
- 用 closure 重建 `styio-nano`
- 对静态仓库 contract 做 producer / consumer 验证
- 对 nano package workflow 做回归测试

原因很简单：这些都直接依赖编译器内部知识，尤其是：

- 哪些源码必须进入 closure
- 哪些 profile 开关映射到哪些 compile definitions
- 哪些 runtime / CLI 能力必须被裁剪

### 5.2 应由 `spio` 接管的职责

这些职责不应长期继续堆在 `styio` 主编译器里：

- registry source 管理
- 远端下载/重试/镜像
- 本地缓存目录布局
- pin / install / use / vendor
- project-level toolchain selection
- package search / registry UX
- 发布流程中的权限与分发策略

换句话说：

- `styio` 更像 nano package 的**producer + verifier**
- `spio` 应成为 nano package 的**distributor + installer + lifecycle manager**

---

## 6. `spio` 后续需要使用的 `styio` 接口

### 6.1 已可用接口

`spio` 现在就可以依赖这些 `styio` 能力：

1. `--machine-info=json`
2. `--nano-create` 本地导出
3. `--nano-create` 静态仓库读取
4. `--nano-publish` 本地静态仓库写入

### 6.2 建议使用方式

建议 `spio` 后续按这个模型调用：

1. 若本地需要生成 nano 包：
   - 调用 `styio --nano-create --nano-mode=local-subset ...`
2. 若需要写入本地 staging repo：
   - 调用 `styio --nano-publish ...`
3. 若需要做 contract self-check：
   - 调用 `styio --nano-create --nano-mode=cloud ...`

这里的重点是：

- `spio` 不需要复制 source closure 逻辑
- `spio` 不需要复制 package-local CMake 生成逻辑
- `spio` 只需要编排 `styio` 已提供的 producer / verifier 接口

---

## 7. 仍然缺少、需要后续联动的部分

这些仍然是后续阶段的工作，不应伪装成“已经完成”：

### 7.1 真正的仓库索引服务语义

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

这些更适合在 `spio` 仓库侧定义。

### 7.2 compile-plan live handoff

`spio build/run/test` 若要真正走 live path，仍需要 `styio` 发布更稳定的 compile-plan consumer / producer 边界。

这部分与 nano package workflow 相关，但不是同一个 contract，不能混在一起。

### 7.3 更细粒度的 closure 剪裁

当前 local-subset 已经做到 source closure + package-local rebuild，但仍不是“函数级最小闭包”。

因此对 edge device 而言，当前是：

- 已进入可用阶段
- 但仍不是终极最小体积

这属于 `styio` 侧后续继续做的优化项，而不是 `spio` 侧任务。

---

## 8. 推荐的阶段性交接顺序

### Phase A: `styio` 内自举稳定

目标：

- local-subset 可稳定产包
- publish / consume roundtrip 稳定
- machine-info capability 稳定

当前状态：已基本完成。

### Phase B: `spio` 接管本地 lifecycle

目标：

- `spio` 调用 `styio` 生成 nano 包
- `spio` 调用 `styio` 发布到本地 staging repo
- `spio` 调用 `styio` 做 install 验证

### Phase C: `spio` 接管远端分发

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

## 9. 本阶段结论

到当前为止，`styio` 侧已经具备：

- 生成 `styio-nano` 包
- 以 source closure 形式重建 `styio-nano`
- 写入静态仓库
- 从静态仓库读取并安装
- 用测试验证 publish -> consume roundtrip

因此，对 `spio` 的下一步配合方式应当是：

- 不复制 `styio` 的 nano package producer 逻辑
- 直接消费当前静态仓库 contract
- 在 `spio` 仓库内定义更高层的远端 lifecycle 与 UX

这就是当前阶段的 handoff 边界。
