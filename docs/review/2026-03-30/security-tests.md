# Styio 安全测试模块说明

**Purpose:** Record the security-oriented tests and coverage notes for the 2026-03-30 review bundle.

**Last updated:** 2026-04-08

本文档**仅描述安全测试子模块**：源码位置、构建与运行方式、用例清单、依赖的被测符号，以及与编译器其余部分的变更对照。  
静态审查中的待改进项、词法器代码级修复明细见同目录下的 [findings.md](./findings.md) 与 [fixes-applied.md](./fixes-applied.md)；**不与上述文档混写同一章节**。

---

## 1. 模块定位

| 项目 | 说明 |
|------|------|
| **目的** | 对工具链中与**解析边界、拒绝服务、运行时 FFI** 相关的性质做可重复回归，避免改坏后静默回归。 |
| **范围** | 当前以 **Google Test** 单进程单测为主：直接调用 `styio_core` 内符号（词法、部分 `ExternLib`），**不**替代里程碑 stdout 金样与 `PipelineCheck` 全管线测试。 |
| **非目标** | 不在此文档展开通用代码审查结论；不替代模糊测试（AFL/libFuzzer）或进程级沙箱测试（可归入后续扩展）。 |

---

## 2. 目录与构建产物

| 路径 | 角色 |
|------|------|
| `tests/security/styio_security_test.cpp` | 全部 `TEST(...)` 实现与本地辅助函数（如 `free_tokens`） |
| `tests/CMakeLists.txt` | 目标 `styio_security_test`、`gtest_discover_tests`、CTest 标签 |
| 仓库根 `CMakeLists.txt` | `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 指向源码根，故可执行文件通常生成在**仓库根目录**（与 `styio` 并列），名为 `styio_security_test` |

**CMake 要点（`tests/CMakeLists.txt`）**

- `add_executable(styio_security_test security/styio_security_test.cpp)`
- `target_link_libraries(styio_security_test PRIVATE styio_core GTest::gtest_main)`
- `gtest_discover_tests(styio_security_test WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTIES LABELS "security;lexer;ffi")`

---

## 3. 用例清单

所有用例均定义于 `tests/security/styio_security_test.cpp`（框架为 GoogleTest）。

### 3.1 `StyioSecurityLexer`（词法 / `StyioTokenizer::tokenize`）

| `TEST` 全名 | 验证性质 | 主要依赖 |
|--------------|----------|----------|
| `EmptySourceProducesEof` | 空串仍能产生以 `TOK_EOF` 结束的序列 | `StyioTokenizer::tokenize` |
| `UnterminatedStringThrowsLexError` | 未闭合 `"` 抛 `StyioLexError` | 同上 |
| `UnterminatedBlockCommentThrowsLexError` | 未闭合 `/*` 抛 `StyioLexError` | 同上 |
| `LineCommentAtEofWithoutNewlineDoesNotThrow` | 行注释位于 EOF 且无 `\n` 时应正常结束 tokenization | 同上 |
| `EmbeddedNulByteDoesNotHang` | 含嵌入 `NUL` 的源码须在时限内结束；`std::async` + `wait_for` | 同上 |
| `VeryLongIdentifierCompletes` | 长度 200000 的标识符可完成 token 化 | 同上 |

**辅助符号**：匿名命名空间内 `free_tokens(std::vector<StyioToken*>&)`，用于释放 `StyioToken::Create` 分配的堆对象。

### 3.2 `StyioSecurityRuntime`（FFI / `ExternLib`）

| `TEST` 全名 | 验证性质 | 主要依赖 |
|--------------|----------|----------|
| `StrcatAbAllocatesWithoutPairingFree` | `styio_strcat_ab` 基本拼接；源内注释说明 JIT 路径无配对 `free` 的已知泄漏面 | `src/StyioExtern/ExternLib.cpp` 中 `styio_strcat_ab` |
| `StrcatAbHugeInputDoesNotCrash` | 两侧各约 40000 字符拼接，总长 80000，不崩溃 | 同上 |

**头文件**：`#include "StyioExtern/ExternLib.hpp"`；测试内对 `malloc` 结果使用 `std::free` 仅为单测清理，**不代表**生成程序会释放 JIT 侧结果。

---

## 4. 运行方式

在配置好的构建目录下：

```bash
ctest -L security
```

或直接执行（路径以实际构建输出为准，常见为仓库根）：

```bash
./styio_security_test
```

---

## 5. 与 `Tokenizer` 修复的对照（回归映射）

以下映射用于变更评审时对照：**被测行为** ↔ **已修改的词法实现**（细节见 [fixes-applied.md](./fixes-applied.md)，不重贴代码）。

| 词法修复摘要 | 覆盖用例 |
|--------------|----------|
| `UNKNOWN` 单字节 + `loc += 1`（防未识别字节死循环） | `EmbeddedNulByteDoesNotHang` |
| 标识符 `while` 边界、`switch` 前 `loc` 守卫 | `VeryLongIdentifierCompletes`、`EmbeddedNulByteDoesNotHang`（多 token 序列） |
| 整数/浮点 digit 循环与 `.` 前瞻边界 | 由长数字场景与静态审查共同覆盖；超长纯数字可后续加专用用例 |

**仍为“预期抛异常”的用例**（未闭合 `"` / `/*`）已迁移到结构化 `StyioLexError`；行尾 `//` 在 EOF 情况现为合法输入并生成 `TOK_EOF` 结尾序列。

---

## 6. 后续可扩展方向（模块维护）

- 将畸形输入期望从 `out_of_range` 迁移为项目统一的 `StyioParseError` 或错误码后，同步改写本节用例断言。  
- 增加 `PipelineCheck` / `popen` 路径的**独立**安全用例时：建议仍放在 `tests/security/` 下新源文件，并**只**在本文件追加 § 小节描述，避免写回 findings 长文。
