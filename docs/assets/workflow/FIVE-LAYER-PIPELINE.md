# Styio 五层流水线 Goldens

**文档作用：** 说明 **Lexer → Parser(AST) → StyioIR → LLVM IR → 进程 stdout** 的分层 golden 比对框架、目录约定与维护方式；与里程碑 **仅比最终 stdout** 的 `styio_stdout_golden_test` 互补。

**实现入口：** `src/StyioTesting/PipelineCheck.cpp`（`styio::testing::run_pipeline_case`）  
**自动化：** `styio_test`（GoogleTest，`ctest --test-dir build -L styio_pipeline`）

---

## 1. 五层分别验证什么

| 层 | 内容 | Golden 文件 | 说明 |
|---|------|-------------|------|
| L1 | 词法 | `expected/tokens.txt` | 每行 `getTokName(type)\t` + 转义后的 `original`；流末含 **`TOK_EOF`**（见 §3）。 |
| L2 | AST | `expected/ast.txt` | **类型推断之后** 的 `StyioRepr` 文本（与 CLI `--styio-ast` 中 *Type-Checking* 段一致）；当前由 `nightly` parser 生成。 |
| L3 | StyioIR | `expected/styio_ir.txt` | `StyioIR::toString` |
| L4 | LLVM IR | `expected/llvm_ir.txt` | `Module::print`（无 ANSI）；**与目标 `datalayout` 等绑定**，换机器需更新 golden。 |
| L5 | 运行 | `expected/stdout.txt` | 子进程执行 **`styio --file input.styio`** 的标准输出；若存在 `expected/stderr.txt`，则 stderr 也一并比对。 |

文本比对前会对 **除 token 以外** 的层做「去掉末尾多余换行、再统一加一个 `\n`」的规范化；token golden 也同样规范化，避免编辑器是否带最后一行换行导致失败。

---

## 2. 用例目录布局

```
tests/pipeline_cases/<case_name>/
  input.styio
  stdin.txt              # optional, used only for L5 subprocess stdin
  expected/
    tokens.txt
    ast.txt
    styio_ir.txt
    llvm_ir.txt
    stdout.txt
    stderr.txt             # optional, used only when L5 should also diff stderr
```

新增用例：复制目录、改 `input.styio`，再逐项更新 `expected/*`（可先故意留空跑 `styio_test` 看失败信息；**L1** 可设环境变量 `STYIO_PIPELINE_DUMP_FULL=1` 导出完整 token 串）。

当前仓库内已落地的 five-layer cases：

- `p01_print_add`
- `p02_simple_func`
- `p03_write_file`
- `p04_read_file`
- `p05_snapshot_accum`
- `p06_zip_files`
- `p07_instant_pull`
- `p08_redirect_file`
- `p09_full_pipeline`
- `p10_auto_detect_read`
- `p11_pipe_func`
- `p12_stdin_echo`
- `p13_stdin_transform`
- `p14_stdin_pull`
- `p15_stdin_mixed_output`

其中 `p12_stdin_echo`、`p13_stdin_transform`、`p15_stdin_mixed_output` **刻意使用**
`>> @stdout/@stderr`，用于冻结“标准流资源写入 shorthand 仍被接受”的兼容行为；里程碑冻结文档中的 canonical 教学写法仍是 `-> @stdout/@stderr`。

`p14_stdin_pull` 则冻结了另一条不同契约：`(<< @stdin)` 当前仍沿用旧 `InstantPullAST`
标量路径，会经过 `styio_cstr_to_i64()`，因此该 case 使用数值 stdin fixture，而不是字符串回显。

---

## 3. 词法与 EOF（实现注意）

- **尾字符：** 词法主循环使用 `while (loc < code.length())`；在仅处理空白/换行后若 `loc == code.length()` 则 **`break`**，避免 `compare(at(loc))` 越界。
- **流结束：** 在 `tokenize` 返回前 **追加 `TOK_EOF`**，与 `parse_stmt_or_expr` 对文件结束的处理一致。
- **`getTokName`：** 对 `TOK_PLUS` / `TOK_MINUS` / `TOK_STAR` / `TOK_AMP` 等需有稳定显示名，便于 L1 golden 可读。

---

## 4. 构建与 CMake 要点

- **`styio_core` 静态库**：编译器实现集中于库；`styio` 可执行文件仅链接 `main.cpp` + `styio_core`。
- **LLVM 头文件不得污染 GoogleTest**：`LLVM_INCLUDE_DIRS` / 相关宏仅绑定 **`styio_core`** 与 **`styio`** 目标（`SYSTEM PRIVATE`），否则 `gtest` 会与 `libc++abi` 头冲突。
- **L5 依赖：** `styio_test` 通过 `add_dependencies(styio_test styio)` 确保先有 `styio` 可执行文件；运行器路径由宏 **`STYIO_COMPILER_EXE`**（或环境变量同名）指定。
- **Parser 路由：** `PipelineCheck.cpp` 的 L2 parser 当前固定走 `parse_main_block_with_engine_latest(..., StyioParserEngine::Nightly, ...)`，用于让 five-layer golden 直接约束默认 parser 路径。

---

## 5. 与里程碑测试的关系

- **里程碑：** 仍由 `tests/CMakeLists.txt` 的 `styio_stdout_golden_test` 等负责 **端到端行为**（速度快、覆盖面大）。
- **五层流水线：** 用于 **回归具体编译阶段**；LLVM 层 golden 更重，按需增加用例，避免全语言覆盖在两个框架重复维护。
