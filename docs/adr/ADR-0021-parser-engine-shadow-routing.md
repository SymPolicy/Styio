# ADR-0021: ParserEngine 双轨路由与影子实现

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

E 阶段要求采用 Strangler 模式拆分 parser：`legacy` 稳定路径 + `new` 影子路径并存。
当前代码只有 CLI 参数 `--parser-engine`，但在主程序中硬编码拒绝非 `legacy`，无法形成可演进的双轨路由边界。

## Decision

1. 在 parser 层新增 `StyioParserEngine` 抽象与统一入口：
   - `styio_parse_parser_engine(...)`
   - `parse_main_block_with_engine(...)`
2. `legacy` 路径继续走现有 `parse_main_block(...)`。
3. `new` 路径先接入影子实现 `parse_main_block_new_shadow(...)`，当前暂复用 legacy 逻辑（零行为变化）。
4. 主程序不再手写引擎分支判断，改为调用 parser 层路由入口。
5. 增加回归测试：`legacy/new` 在样例输入上输出一致，非法引擎参数可诊断拒绝。

## Alternatives

1. 继续在 `main.cpp` 中硬编码分支，不引入 parser 层接口。
2. 直接替换默认 parser 为新实现，不保留双轨。
3. 在新 parser 可用前完全禁用 `--parser-engine=new`。

## Consequences

1. E.2-E.5 可以在 `new` 路径内增量演进，不影响 `legacy` 默认行为。
2. CLI 行为更稳定：非法引擎统一按 `CliError` 返回。
3. 当前 `new` 仍是影子复用，后续需逐段替换为真实新 parser 子集实现。
