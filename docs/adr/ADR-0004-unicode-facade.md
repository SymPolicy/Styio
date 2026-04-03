# ADR-0004: Unicode 防腐层与 ICU 隔离边界

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

Styio 前端（Tokenizer/Parser）此前直接调用 `isalpha`/`isalnum`/`isdigit`/`isspace`。这导致：

1. 字符分类逻辑分散，后续替换 ICU/utf8proc/simdutf 成本高。
2. `std::is*` 直接接收 `char` 存在 signed-char UB 风险。
3. 若在前端层直接引入 ICU 类型，会污染 Token/AST/Session 边界。

## Decision

1. 新增 `StyioUnicode` 模块（`src/StyioUnicode/Unicode.hpp/.cpp`）作为唯一 Unicode 接口层。
2. 前端改为仅调用 `StyioUnicode::*`，不再直接调用 `std::is*`。
3. `StyioUnicode` 对上层只暴露标准类型（`char` / `uint32_t` / `string_view`），不泄漏 ICU 类型。
4. ICU 后端仅在实现层通过 `STYIO_USE_ICU` 条件编译启用；默认保持可选并关闭。

## Alternatives

1. 维持现状（前端直接调用 `std::is*`）。
2. 在 Parser/Tokenizer 中直接调用 ICU API。
3. 立即替换 ICU 为新库（utf8proc/simdutf）并同时重构前端。

## Consequences

1. 未来替换 Unicode 后端只需改 `StyioUnicode` 实现层，上层语义基本不动。
2. 消除了 `std::is*` 的 signed-char UB 风险点。
3. 本次不改变语言语义：标识符仍以现有字节级规则为主；全面 Unicode 标识符开放作为后续增量里程碑。
