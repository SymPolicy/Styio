# 2026-03-30 审查后已落地的修复明细

**Purpose:** Record the fixes applied in response to the 2026-03-30 review findings.

**Last updated:** 2026-04-08

本文档**仅**逐项列出**编译器源码中**已修改的部分：`Tokenizer.cpp` 内的词法行为、动机与代码引用。

- 静态审查待办见 [findings.md](./findings.md)。
- **安全测试模块**（用例、CMake、运行方式、与词法修复的对照）见独立文档 [security-tests.md](./security-tests.md)，本文不包含安全测试章节。

未修改的项（例如 `main.cpp` 的 `text += EOF`、`StyioContext` 泄漏等）仅保留在 [findings.md](./findings.md)。

---

## 1. 词法模块：`StyioTokenizer::tokenize`

| 字段 | 说明 |
|------|------|
| **构建目标** | 静态库 `styio_core` |
| **源文件** | `src/StyioParser/Tokenizer.cpp` |
| **头文件（声明）** | `src/StyioParser/Tokenizer.hpp`（本次未改声明，仅实现） |
| **函数** | `StyioTokenizer::tokenize(std::string code)`（成员函数，返回 `std::vector<StyioToken*>`） |

### 1.1 `default` 分支：未知单字节不再卡死循环

- **位置**：`tokenize` 内部主循环末尾的 `switch (code.at(loc))` 中，原 `default: break;`。
- **问题**：`loc` 不增加，若当前字节未在任何 `case` 中处理（典型：`'\0'` / 嵌入 `NUL`），外层 `while (loc < code.length())` 永不前进 → **拒绝服务（死循环）**。
- **修复**：`default` 改为生成 `StyioTokenType::UNKNOWN`、词素为单字符 `std::string(1, code.at(loc))`，并执行 `loc += 1`。
- **代码位置**：

```525:530:src/StyioParser/Tokenizer.cpp
      default: {
        /* Single-byte not recognized above (e.g. embedded NUL): must advance. */
        tokens.push_back(
          StyioToken::Create(StyioTokenType::UNKNOWN, std::string(1, code.at(loc))));
        loc += 1;
      } break;
```

- **依赖类型**：`StyioToken::Create`、`StyioTokenType::UNKNOWN`（定义于 `src/StyioToken/Token.hpp`）。

### 1.2 标识符扫描：`while` 条件增加上界

- **位置**：`/* varname / typename */` 分支内的 `do { ... } while (...)`。
- **问题**：原条件在循环尾使用 `isalnum(code.at(loc))` 等；当标识符恰好延伸到缓冲区末尾时，`loc == code.length()` 仍调用 `at(loc)` → **`std::out_of_range`**。
- **修复**：在 `while` 条件最前增加 `loc < code.length() &&`，再读取 `code.at(loc)`。

```100:104:src/StyioParser/Tokenizer.cpp
      do {
        literal += code.at(loc);
        loc += 1;
      } while (loc < code.length()
               && (isalnum(code.at(loc)) || (code.at(loc) == '_')));
```

### 1.3 整数 / 浮点literal：数字化循环与 `float` 前瞻

- **位置**：`/* integer / float / decimal */` 分支。
- **子项 A — 整数部分 `do-while`**：与 1.2 同类，在末尾判断 `isdigit(code.at(loc))` 前增加 `loc < code.length() &&`。

```117:120:src/StyioParser/Tokenizer.cpp
      do {
        literal += code.at(loc);
        loc += 1;
      } while (loc < code.length() && isdigit(code.at(loc)));
```

- **子项 B — 判断是否形如 `xxx.yyy`**：原为 `code.at(loc) == '.' && isdigit(code.at(loc + 1))`，在 `loc == size()-1` 或 `loc == size()` 时可能对 `at(loc)` / `at(loc+1)` 越界。
- **修复**：改为 `loc + 1 < code.length() && code.at(loc) == '.' && isdigit(code.at(loc + 1))`。

```122:123:src/StyioParser/Tokenizer.cpp
      /* If Float: xxx.yyy */
      if (loc + 1 < code.length() && code.at(loc) == '.' && isdigit(code.at(loc + 1))) {
```

- **子项 C — 小数部分 `do-while`**：同样在 `while` 中加 `loc < code.length() &&`。

```129:132:src/StyioParser/Tokenizer.cpp
        do {
          literal += code.at(loc);
          loc += 1;
        } while (loc < code.length() && isdigit(code.at(loc)));
```

### 1.4 进入标点 `switch` 前的守卫

- **位置**：在 `switch (code.at(loc))` 之前（紧接在标识符/数字 `if / else if` 块之后）。
- **问题**：当本轮循环仅消费了完整标识符或整数且已到达 `loc == code.length()` 时，仍会执行 `switch (code.at(loc))` → 越界。
- **修复**：若 `loc >= code.length()`，则 `continue` 进入下一轮 `while`（最终由循环退出逻辑追加唯一尾 `TOK_EOF`）。

```141:143:src/StyioParser/Tokenizer.cpp
    if (loc >= code.length()) {
      continue;
    }
```

---

## 2. 审查文档索引（非代码）

| 文件 | 内容 |
|------|------|
| `docs/review/2026-03-30/findings.md` | 审查发现与未closure项 |
| `docs/review/2026-03-30/security-tests.md` | **安全测试模块专档**（用例、构建、运行、与 §1 的对照） |
| `docs/review/2026-03-30/README.md` | 审查目录入口 |

针对 §1 的自动化回归说明见 [security-tests.md](./security-tests.md) 第 5 节，**不重复**于本文。

---

## 3. 明确未随词法补丁修改的编译器文件

以下文件在审查中被讨论，但**不在**本文 §1 词法修复范围内（未改）：

- `src/main.cpp`
- `src/StyioParser/Parser.hpp` / `StyioContext`
- `src/StyioExtern/ExternLib.cpp`（`styio_strcat_ab` 所有权策略未改）
- `src/StyioTesting/PipelineCheck.cpp`
- `src/StyioJIT/StyioJIT_ORC.hpp`

安全测试相关变更见 [security-tests.md](./security-tests.md)，**不在** `fixes-applied.md` 复述。
