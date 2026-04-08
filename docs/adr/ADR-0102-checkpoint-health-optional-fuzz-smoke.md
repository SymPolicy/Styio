# ADR-0102: Checkpoint Health 接入可选 Fuzz Smoke

- Status: Accepted
- Date: 2026-04-07

## Context

在 `ADR-0101` 后，`styio_fuzz_parser` 已经会同时驱动 legacy/nightly 两条 parser 路由，但 `scripts/checkpoint-health.sh` 仍只覆盖普通构建、soak、shadow gate 和 sanitizer，不会碰 fuzz smoke。

这会让恢复脚本与当前验证现实继续脱节：

1. 本地存在 `build-fuzz` 时，恢复脚本仍不会顺手检查 fuzz smoke；
2. 没有 fuzz 构建的环境，又不应该被强行要求配置 fuzz 工具链。

## Decision

1. `scripts/checkpoint-health.sh` 新增：
   - `--fuzz-build-dir <dir>`
   - `--no-fuzz`
2. 默认模式为自动探测：
   - 若 `build-fuzz` 下存在 CTest 元数据，则自动运行 `fuzz_smoke`
   - 若不存在，则打印 skip 信息，不使脚本失败
3. 用户显式传入 `--fuzz-build-dir` 时，视为强要求：
   - 脚本会构建 `styio_fuzz_lexer` / `styio_fuzz_parser`
   - 然后运行 `ctest -L fuzz_smoke`
4. `--no-fuzz` 可显式关闭 fuzz 段。

## Alternatives

1. 强制所有环境都必须有 fuzz build：
   - 拒绝。过于苛刻，会让恢复脚本失去通用性。
2. 继续完全不让 `checkpoint-health` 触达 fuzz：
   - 拒绝。fuzz smoke 已经是当前 parser 稳定窗口的一部分。
3. 单独再写一份 `checkpoint-health-fuzz.sh`：
   - 拒绝。恢复入口应该尽量单一。

## Consequences

1. 有 `build-fuzz` 的环境下，`checkpoint-health --no-asan` 现在会自动多过一层 `fuzz_smoke`。
2. 没有 fuzz 构建的环境仍可使用同一脚本，只是会收到明确的 skip 提示。
3. fuzz 验证首次进入 checkpoint 恢复路径，后续 fuzz 回流样本和 nightly parser 收敛将更容易对齐到同一健康面板。
