# Styio 编译器代码审查（2026-03-30）

本目录记录对 Styio C++ 编译器前端的静态审查结论，侧重：**逻辑与 API 误用、资源与内存生命周期、边界与拒绝服务风险、CLI/测试中的命令构造安全**。

详细条目见 [findings.md](./findings.md)。**词法等相关代码修复明细**见 [fixes-applied.md](./fixes-applied.md)。**安全测试子模块**（独立文档，不混入上述文件）见 [security-tests.md](./security-tests.md)。

## 摘要

| 类别 | 结论 |
|------|------|
| 内存安全（词法） | 块注释、字符串、行尾 `//` 等在缺闭合/缺换行时仍可能 **`out_of_range`**；`UNKNOWN` 字符（含嵌入 `NUL`）已通过推进 `loc` 避免**死循环**（2026-03-30 小修复）。 |
| 内存生命周期 | `main` 路径对 `StyioContext`、全部 `StyioToken*`、`MainBlockAST`、`StyioIR` 等**无统一释放**；`StyioContext` 内部 `new StyioRepr()` **无对应 delete**（见 `Parser.hpp`）。 |
| 运行时 FFI | `styio_strcat_ab` 使用 `malloc` 返回指针，生成的 IR **未见配对 `free`**，长链字符串拼接存在**堆累积泄漏**。`FILE*` 经 `int64` 传递，错误句柄可导致 **fclose 未定义行为**。 |
| JIT 暴露面 | `DynamicLibrarySearchGenerator::GetForCurrentProcess` 将进程内大量符号暴露给 JIT 链接器；配合不受信 LLVM IR 时扩大**意外符号解析**面（依赖威胁模型评估）。 |
| 测试/自动化 | `PipelineCheck` 将路径拼进 `bash -c` 字符串，含引号/换行等字符时存在**命令注入**构造风险（仅影响跑测试的一方，但违背纵深防御）。 |

安全测试目录为 `tests/security/`，说明与用例索引以 [security-tests.md](./security-tests.md) 为准。
