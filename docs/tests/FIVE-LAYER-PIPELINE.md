# Styio 五层流水线 Goldens

**文档作用：** 说明 **Lexer → Parser(AST) → StyioIR → LLVM IR → 进程 stdout** 的分层 golden 比对框架、目录约定与维护方式；与里程碑 **仅比最终 stdout** 的 `styio_stdout_golden_test` 互补。

**实现入口：** `src/StyioTesting/PipelineCheck.cpp`（`styio::testing::run_pipeline_case`）  
**自动化：** `styio_test`（GoogleTest，`ctest --test-dir build -L styio_pipeline`）

---

## 1. 五层分别验证什么

| 层 | 内容 | Golden 文件 | 说明 |
|---|------|-------------|------|
| L1 | 词法 | `expected/tokens.txt` | 每行 `getTokName(type)\t` + 转义后的 `original`；流末含 **`TOK_EOF`**（见 §3）。 |
| L2 | AST | `expected/ast.txt` | **类型推断之后** 的 `StyioRepr` 文本（与 CLI `--styio-ast` 中 *Type-Checking* 段一致）。 |
| L3 | StyioIR | `expected/styio_ir.txt` | `StyioIR::toString` |
| L4 | LLVM IR | `expected/llvm_ir.txt` | `Module::print`（无 ANSI）；**与目标 `datalayout` 等绑定**，换机器需更新 golden。 |
| L5 | 运行 | `expected/stdout.txt` | 子进程执行 **`styio --file input.styio`** 的标准输出（因 `printf` 不走 `std::cout`，必须与 CLI 一致）。 |

文本比对前会对 **除 token 以外** 的层做「去掉末尾多余换行、再统一加一个 `\n`」的规范化；token golden 也同样规范化，避免编辑器是否带最后一行换行导致失败。

---

## 2. 用例目录布局

```
tests/pipeline_cases/<case_name>/
  input.styio
  expected/
    tokens.txt
    ast.txt
    styio_ir.txt
    llvm_ir.txt
    stdout.txt
```

新增用例：复制目录、改 `input.styio`，再逐项更新 `expected/*`（可先故意留空跑 `styio_test` 看失败信息；**L1** 可设环境变量 `STYIO_PIPELINE_DUMP_FULL=1` 导出完整 token 串）。

---

## 3. 词法与 EOF（实现注意）

- **尾字符：** 词法主循环使用 `while (loc < code.length())`；在仅处理空白/换行后若 `loc == code.length()` 则 **`break`**，避免 `compare(at(loc))` 越界。
- **流结束：** 在 `tokenize` 返回前 **追加 `TOK_EOF`**，与 `parse_stmt_or_expr` 对文件结束的处理一致。
- **`getTokName`：** 对 `TOK_PLUS` / `TOK_MINUS` / `TOK_STAR` 等需有稳定显示名，便于 L1 golden 可读。

---

## 4. 构建与 CMake 要点

- **`styio_core` 静态库**：编译器实现集中于库；`styio` 可执行文件仅链接 `main.cpp` + `styio_core`。
- **LLVM 头文件不得污染 GoogleTest**：`LLVM_INCLUDE_DIRS` / 相关宏仅绑定 **`styio_core`** 与 **`styio`** 目标（`SYSTEM PRIVATE`），否则 `gtest` 会与 `libc++abi` 头冲突。
- **L5 依赖：** `styio_test` 通过 `add_dependencies(styio_test styio)` 确保先有 `styio` 可执行文件；运行器路径由宏 **`STYIO_COMPILER_EXE`**（或环境变量同名）指定。

---

## 5. 与里程碑测试的关系

- **里程碑：** 仍由 `tests/CMakeLists.txt` 的 `styio_stdout_golden_test` 等负责 **端到端行为**（速度快、覆盖面大）。
- **五层流水线：** 用于 **回归具体编译阶段**；LLVM 层 golden 更重，按需增加用例，避免全语言覆盖在两个框架重复维护。
