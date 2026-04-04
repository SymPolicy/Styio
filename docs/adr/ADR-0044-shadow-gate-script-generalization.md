# ADR-0044: Shadow Gate 脚本通用化命名

- **Status:** Accepted
- **Date:** 2026-04-04

## Context

原脚本名 `parser-shadow-m1-gate.sh` 已被 M1 与 M2 共同使用，命名与实际职责不一致，增加了维护与理解成本。

## Decision

1. 新增通用脚本：`scripts/parser-shadow-suite-gate.sh`。
2. 保留 `scripts/parser-shadow-m1-gate.sh` 作为兼容包装：
   - 直接转发到新脚本。
3. CI 与文档统一切换到新脚本名。

## Alternatives

1. 继续沿用旧名称，不做命名收敛。
2. 删除旧脚本并强制所有调用立即切换。
3. 为 M2 再新增一个 `m2` 专用脚本。

## Consequences

1. 脚本职责更清晰，减少误解。
2. 兼容层避免一次性破坏现有调用方。
