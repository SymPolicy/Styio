# ADR-0066: `mark_cur_tok` 边界防护与 fuzz smoke 稳定运行约束

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

在 `STYIO_ENABLE_FUZZ=ON` 的 parser smoke 中，发现两类阻断问题：

1. 真实崩溃：`parse_stmt_or_expr` 异常路径调用 `mark_cur_tok(...)` 时，`token_coordinates/index_of_token` 越界导致崩溃。
2. 工具链噪音：macOS + clang-18 + libFuzzer 组合下，`fuzz_parser_smoke` 在 libFuzzer 自身目录扫描阶段触发 `ASan container-overflow` 误报，非 Styio 目标代码缺陷。

此外，`styio_fuzz_parser` 单独构建时缺少 LLVM 头文件可见性，导致 `AST.hpp` 的 LLVM include 失败。

## Decision

1. 给 `StyioContext::mark_cur_tok` 增加完整边界检查：
   - 越界时降级返回纯文本注释，不再二次崩溃。
2. 固化 fuzz 发现样本：
   - 新增 parser corpus seed：`seed-malformed-stmt-prefix.styio`。
   - 新增单测：`MalformedStatementPrefixReportsParseErrorWithoutCrash`。
3. 修正 fuzz 目标编译上下文：
   - `styio_fuzz_lexer/parser` 显式继承 `LLVM_INCLUDE_DIRS` 与 `LLVM_DEFINITIONS_LIST`。
4. 对 fuzz smoke 测试设置：
   - `ASAN_OPTIONS=detect_container_overflow=0`，避免 libFuzzer 运行时误报干扰 PR 级 smoke 稳定性。

## Alternatives

1. 仅在 `parse_stmt_or_expr` 局部移除 `mark_cur_tok` 调用。
2. 保持 fuzz smoke 原样，接受在当前工具链下随机红灯。
3. 将 fuzz smoke 完全迁移到 nightly，不在本地/PR 跑。

## Consequences

1. parser 错误路径从“可能崩溃”收敛为稳定 `ParseError`。
2. fuzz smoke 能稳定覆盖 Styio 本体，而非被工具链噪音阻断。
3. 若未来工具链升级消除该误报，可去掉 `detect_container_overflow=0` 并恢复更严格 ASan 行为。
