# ADR-0049: 将 Hash 新覆盖形态纳入 M2 Golden 与 Shadow Gate

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0049: 将 Hash 新覆盖形态纳入 M2 Golden 与 Shadow Gate.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

`ADR-0047/0048` 已让 NewParser 支持：
- `# f(...) : T = expr`
- `# f : T => expr` / `# f => expr`

如果只保留在单元测试中，这些形态不会进入里程碑 golden 与 `parser-shadow-suite-gate` 的长期观测窗口，回归发现会滞后。

## Decision

1. 在 `tests/milestones/m2/` 新增样例 `t11_hash_assign_and_arrow.styio`。
2. 新增对应 golden：
   - `tests/milestones/m2/expected/t11_hash_assign_and_arrow.out`。
3. 更新 `tests/milestones/m2/EXPECTED.txt`，并让该样例自动纳入：
   - `ctest -L m2`
   - M2 full shadow gate（按 `t*.styio` 扫描）。

## Alternatives

1. 仅依赖 `tests/security` / `tests/styio_test` 覆盖，不进里程碑目录。
2. 放到 `m3+` 再覆盖，延后验证。
3. 单独维护非里程碑 gate 脚本，不使用现有 M2 体系。

## Consequences

1. M2 里程碑覆盖扩展到 10 个用例，新增 hash 形态进入常驻门禁。
2. M2 shadow gate 统计从 `18/18` 提升为 `20/20`（双引擎双运行）。
3. 新解析路径在 golden/CI/nightly 的可见性更高，回归诊断更快。
