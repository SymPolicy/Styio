# ADR-0031: Parser 默认切换门槛（E.6 首段）

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

`new` parser 已进入 shadow compare 阶段，并且 M1 全量样例可通过；但默认行为仍未切换。E.6 需要把“何时允许切默认”从口头约定变成可执行门槛，避免分支漂移后门槛失真。

## Decision

1. 在 CI 中加入显式 gate：
   - `StyioParserEngine.ShadowCompareAcceptsM1FullSuite` 必须通过。
2. 在 gate 长期稳定前，默认解析引擎继续保持 `legacy`。
3. 默认切换到 `new` 的前置条件至少包含：
   - shadow gate 稳定通过（无新增差异）；
   - 出现差异时可产出可回放工件（`--parser-shadow-artifact-dir`）。

## Alternatives

1. 仅依赖 `styio_pipeline` 总标签间接覆盖，不设置显式 gate。
2. 立即切换默认到 `new`，再按反馈回补差异。
3. 等到 NewParser 覆盖所有语法后一次性设置 gate。

## Consequences

1. parser 默认切换具备可审计的自动化门槛。
2. PR 失败定位更直接，减少“隐式包含但不易观察”的排查时间。
3. 切默认节奏仍保守，但工程风险更可控。
