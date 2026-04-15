# 详细发现清单

**Purpose:** Record the detailed review findings for the 2026-03-30 review bundle.

**Last updated:** 2026-04-08

## 1. 词法分析（`Tokenizer.cpp`）

### 1.1 `EOF` 与 `char` 混用

- `StyioTokenType::TOK_EOF` 在 `Token.hpp` 中被定义为 `-1`，与 C 库宏 `EOF`（通常为 `-1`）混在同一套分支里：`switch (code.at(loc))` 使用 `case EOF:`。
- `std::string` 的元素类型为 `char`。将 `EOF`（`int`）与 `char` 比较在“误把 `char` 提升为 `int`”时可能看似可行，但 **`code.at(loc) != EOF`**（第 71–73 行附近）在 `char` 为无符号的平台语义与可移植性上均不清晰；且源文件中**不可能**出现值等于 `EOF` 的“真实字符”。
- **建议**：词法器只应依赖 `loc < code.size()` 作为结束条件，删除对 `EOF` 字符的依赖；文件结束由循环自然退出，最后再发出 `TOK_EOF` token（当前末尾已有 `tokens.push_back(TOK_EOF)`）。

### 1.2 未闭合块注释与字符串 — 越界与拒绝服务

- 块注释：`while (not(code.compare(loc, 2, "*/") == 0)) { literal += code.at(loc); loc += 1; }` 在 `*/` 永不出现时，`loc` 递增直至 `compare` 或 `at` 抛 `std::out_of_range`，或（若实现细节不同）其它未定义行为。**没有行号/友好错误**，且依赖异常穿越 `main`（`main` 未捕异常则直接 `terminate`）。
- 字符串字面量：`while (code.at(loc) != '\"')` 同样在缺闭合引号时越界。
- **建议**：在词法循环内统一使用带 `loc < size` 的读取辅助函数；未闭合则返回明确错误 token 或抛 `StyioParseError`，由顶层捕获并 `exit(1)`。

### 1.3 未知字节与嵌入 `NUL`（已做最小修复）

- 原实现中 `switch (code.at(loc))` 的 `default: break` **不增加 `loc`**，遇 `\0` 等未列出的单字节会在 `while (loc < code.length())` 中**死循环**（拒绝服务）。
- **现状**：`default` 分支改为发出 `UNKNOWN` 单字符 token 并 `loc += 1`（见 `Tokenizer.cpp`）；回归方式见 [security-tests.md](./security-tests.md)。

### 1.4 标识符/数字消费后的尾部越界（已做最小修复）

- `do { ... } while (isalnum(code.at(loc)) ...)` 与数字分支同类循环在 `loc == size()` 时仍调用 `at(loc)`；名称消费完后未 `continue` 即进入 `switch (code.at(loc))`，在**文件以标识符结尾**时同样越界。
- **现状**：循环条件改为带 `loc < code.length()`；`float` 判定改为 `loc + 1 < code.length()`；在进入 `switch` 前若 `loc >= length()` 则 `continue`（见 `Tokenizer.cpp`）。

### 1.5 行注释在流末无换行

- `//` 分支中 `while (code.at(loc) != '\n' && ...)` 在最后一行无 `\n` 时，`loc` 至 `size()` 会触发 `at` 抛异常。
- **建议**：同上，显式边界检查（仍未改）。当前安全测试对“预期异常”的约定见 [security-tests.md](./security-tests.md)。

## 2. 主程序与源文件读取（`main.cpp`）

### 2.1 `read_styio_file` 中的 `text += EOF`

- `text += EOF` 将 `int` 追加进 `string`：发生**整型到 char 的转换**，行为依赖实现，且与后续词法假设不一致。
- **建议**：删除该行；若需哨兵应使用文档化的约定或根本不追加。

### 2.2 打开文件失败仍继续

- `if (!file.is_open())` 仅打印，未 `return`；后续 `getline` 不会读入内容，但调用方可能仍把空串当合法程序处理。
- **建议**：打开失败时返回错误或设置标志，由 `main` 非零退出。

### 2.3 资源泄漏（进程级）

- `StyioContext::Create` 返回裸指针，**无 `delete`**。
- `tokenize` 返回的 `vector<StyioToken*>` 中每个元素 `new` 分配，`main` **未释放**。
- `parse_main_block` 返回的 AST、`toStyioIR` 返回的 IR 同样未见释放。
- **影响**：单次编译即泄漏；CI 或长期运行的语言服务会放大问题。
- **建议**：`unique_ptr`/`shared_ptr` 或统一 `StyioCompilation` RAII 对象在析构里释放 token/AST/IR/context。

## 3. 解析上下文（`Parser.hpp` / `StyioContext`）

### 3.1 `ast_repr = new StyioRepr()` 未释放

- 每个 `StyioContext` 构造时 `new StyioRepr`，类中**未声明析构函数**释放，也未使用 `unique_ptr`。
- **建议**：改为 `std::unique_ptr<StyioRepr>` 或在析构中 `delete ast_repr`。

## 4. 外部库与 FFI（`ExternLib.cpp`）

### 4.1 `styio_strcat_ab` 堆所有权

- 成功路径返回 `malloc` 缓冲区；失败返回字面量 `""`。生成的 LLVM IR 对字符串 `+` 调用该函数后**没有生成 `free`**（检索 `CodeGenG.cpp` 仅 `CreateCall`，无配套释放）。
- **影响**：程序中频繁拼接字符串时**线性甚至超线性堆占用**，工具类场景下构成可用性/DoS 风险。
- **建议**：提供 `styio_free(void*)` 并在 codegen 插入；或改用**arena / bump allocator**；或改为写入由调用方拥有的缓冲（Breaking API）。

### 4.2 `styio_file_open` 等遇错 `exit(1)`

- 库式 API 使用 `exit` 无法由嵌入宿主捕获；也不便测试。
- **建议**：返回错误码或抛 C++ 异常（若在宿主可接受），与 `main` CLI 分离。

### 4.3 句柄与 `FILE*`

- `int64_t` 与 `uintptr_t` 互转保存 `FILE*`。若 JIT 侧句柄被算错或伪造，可能对任意地址调用 `fclose`。
- **建议**：内部维护 `unordered_map<int, FILE*>` 单调句柄表，校验后再关闭。

## 5. JIT（`StyioJIT_ORC.hpp`）

- `GetForCurrentProcess` 暴露当前进程动态符号空间。
- **风险**：若未来接受**不可信** `.styio` 生成的 IR 或混编插件，链接器可能绑定到非预期符号。
- **建议**：对非调试构建使用**严格符号白名单**（头文件中已有注释掉的 AllowList 雏形，可产品化）。

## 6. 测试基础设施（`PipelineCheck.cpp`）

- Layer 5：`"\"" + layer5_compiler_exe + "\" --file \"" + input.string() + "\" 2>/dev/null"` 经 `popen` / `bash -c` 执行。
- **风险**：`input.string()` 或 exe 路径含 `"`、`` ` ``、`$()` 等时可破坏引用边界（对恶意**测试目录命名**而言是安全测试关注点）。
- **建议**：使用 `execve` 风格向量参数，避免 shell；或 `posix_spawn` + 参数数组。

## 7. 其它小项

- `show_code_with_linenum` 以值传递 `tmp_code_wrap`，大文件时拷贝成本高（非安全，属性能）。
- `Token.hpp` 中大量 `static const` 大表格在每个 TU 可能重复（链接期合并取决于 `inline`；当前为 ODR 与二进制体积问题）。

---

## 文档索引（索引不替代专档正文）

| 文档 | 用途 |
|------|------|
| [fixes-applied.md](./fixes-applied.md) | 已在 `Tokenizer.cpp` 落地的词法修复明细 |
| [security-tests.md](./security-tests.md) | **安全测试独立模块**（源码路径、CMake、用例表、运行方式、与词法修复的对照） |
