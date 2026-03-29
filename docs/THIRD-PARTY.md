# Styio 外部与开源依赖清单

**文档作用：** 列出本仓库 **构建期/运行期** 所依赖的全部 **外部包与开源组件**；声明 **获取方式、用途、许可** 及在仓库中的 **落地位置**。新增依赖时请同步更新本文件，并在 `DOCUMENTATION-POLICY.md` 索引中保持可发现。

**Last updated:** 2026-03-29

---

## 1. 总览（按类别）

| 名称 | 类型 | 构建中角色 | 在仓库中的记录 / 落地 |
|------|------|------------|------------------------|
| **LLVM** | 系统安装 + CMake `find_package` | 编译器后端：IR、ORC JIT、原生目标 | `CMakeLists.txt`（`find_package(LLVM 18.1.0)`，`llvm_map_components_to_libnames` → `support` `core` `irreader` `orcjit` `native`） |
| **ICU**（`uc`、`i18n`） | 系统安装 + `find_package` | Unicode / 字符串（经 `ICU::uc` `ICU::i18n` 链接） | `CMakeLists.txt`（`find_package(ICU COMPONENTS uc i18n)`）；**查找模块** 见仓库根目录 `FindICU.cmake`（源自 CMake 官方模块） |
| **GoogleTest** | **FetchContent**（仅测试） | `styio_test`、五层流水线等单元/集成测试 | `tests/CMakeLists.txt`（`FetchContent_Declare(googletest URL ...)`） |
| **cxxopts** | **随仓单头文件（vendored）** | `styio` 命令行解析 | `src/include/cxxopts.hpp`（**勿随意修改**，见 `docs/AGENT-SPEC.md`） |
| **Clang / LLD / llvm 工具链** | 宿主页 / PATH | 当前工程 `CMakeLists.txt` 写死了编译器与链接器路径（**环境约束**，非常规 Fetch 包） | `CMakeLists.txt`：`CMAKE_CXX_COMPILER`、`CMAKE_LINKER`、`CMAKE_OBJDUMP` |

**运行时：** 生成的 `styio` 可执行文件依赖 **动态链接** 的 ICU 与 LLVM 相关库（以本机 `ldd` / 发行版包为准）；JIT 执行阶段还依赖 **LLVM 已启用的原生目标**。

---

## 2. 逐项说明

### 2.1 LLVM

- **版本约束：** CMake 中要求 **18.1.0**（`LLVM_DIR` / `LLVMConfig.cmake` 由系统或手动安装的 LLVM 提供）。
- **用途：** IR 构建、模块校验、ORC JIT、本机代码生成。
- **许可：** Apache License 2.0 with LLVM Exceptions（以发行版随附为准）。
- **官方：** https://llvm.org/

### 2.2 ICU

- **组件：** `uc`、`i18n`（未在工程中使用 `data` 等其它组件时仍可能作为传递依赖被发行版拉出）。
- **用途：** 与 Unicode 相关的 C API（具体调用点以 `StyioExtern`、`CodeGenIO` 等为线索检索）。
- **许可：** 以 unicode-org/icu 及发行版说明为准（常见为 Unicode License 与第三方组件的组合）。
- **官方：** https://icu.unicode.org/

### 2.3 GoogleTest

- **获取：** CMake `FetchContent`，固定 zip：  
  `https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip`
- **范围：** **仅** `tests/` 下目标（如 `styio_test`）；主目标 `styio` / `styio_core` 不链接 gtest。
- **许可：** BSD 3-Clause（GoogleTest 项目默认）。
- **官方：** https://github.com/google/googletest

### 2.4 cxxopts

- **形式：** 单头文件 `src/include/cxxopts.hpp`，随仓库分发（vendored）。
- **版权信息：** 文件头注明 Copyright (c) 2014-2022 Jarryd Beck，**MIT License**。
- **官方来源（参考）：** https://github.com/jarro2783/cxxopts  
  （当前以仓内副本为准；升级时请替换整文件并更新本段版本/commit 说明若需要。）

### 2.5 FindICU.cmake

- **形式：** 仓库根目录 `FindICU.cmake`，与 CMake 自带的 **FindICU** 模块同源风格（文件头为 OSI-approved BSD 3-Clause）。
- **用途：** 在 `CMakeLists.txt` 中通过 `find_package(ICU ...)` 解析 ICU；`message(STATUS "[ICU] Using FindICU.cmake in: ${PROJECT_SOURCE_DIR}")` 指向本文件。

---

## 3. 非「包」但相关的说明

- **C++ 标准库：** 由工具链提供（如 `libstdc++` / `libc++`），无单独 Fetch。
- **标准 C 库与系统调用：** 运行时 I/O、JIT 加载等依赖 OS。
- **里程碑测试数据：** `tests/milestones/**` 下 `.styio` 与 golden 为 **本仓库自有测试资源**，不是第三方包。

---

## 4. 维护约定

1. 新增 **FetchContent / git submodule / Conan / vcpkg** 等依赖前，在本文件增加一行，并注明 **许可与用途**。
2. 升级 **GoogleTest 固定 URL** 时，同步更新上表中的链接与（如需要）**安全审计**记录。
3. 文档索引：`DOCUMENTATION-POLICY.md` 中「单一事实来源」表应链接到本文件。
