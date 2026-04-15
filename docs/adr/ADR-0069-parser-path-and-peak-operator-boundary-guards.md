# ADR-0069: `parse_path` 与 `peak_operator` 边界防护

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0069: `parse_path` 与 `peak_operator` 边界防护.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-06

## Context

F.3 边界巡检中发现两处 parser 内部异常泄露点：

1. `parse_path` 对 Windows 盘符判断直接访问 `text.at(1)`；当路径内容为单字符（如 `"A"`）时抛 `std::out_of_range("basic_string")`。
2. `peak_operator` 在 EOF/空输入时使用 `code.at(tmp_pos)`，同样抛出 `std::out_of_range("basic_string")`。

对应红灯测试：

- `StyioSecurityParserPath.SingleLetterPathDoesNotThrowOutOfRange`
- `StyioSecurityParserContext.PeakOperatorAtEofReturnsEofWithoutThrow`

## Decision

1. `parse_path` 的 Windows 盘符识别改为长度安全检查：`text.size() >= 2` 才判断 `X:` 形式。
2. `parse_path` 在扫描路径字面量时增加 EOF 边界判断；遇到未闭合引号时抛 `StyioSyntaxError`，不进入死循环。
3. `peak_operator` 重写为边界安全扫描：
   - 所有字符访问改为 `tmp_pos < code.size()` 前置判断；
   - 注释/空白/标识符跳过路径不再使用无边界 `at()`；
   - 扫描到 EOF 时统一返回 `"EOF"`。

## Alternatives

1. 仅在顶层 `main` 捕获 `std::out_of_range`（会泄露内部异常语义，且无法防止潜在循环）。
2. 继续依赖 fuzz 发现并逐例修补调用点（恢复成本高，不利于 checkpoint 快速恢复）。
3. 删除 `peak_operator`（当前仍在代码库中，直接删除风险高于边界加固）。

## Consequences

1. parser 错误路径不再暴露 STL 越界异常文案，诊断语义保持在 Styio 错误体系内。
2. 对“单字符路径 / EOF 上下文”场景的行为稳定化，可被 security/safety 回归长期守门。
3. 与 B.4/B.5 fuzz 目标一致：同类输入不再触发 `out_of_range` 崩溃。
