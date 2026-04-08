# ADR-0059: Nightly Sanitizer Gate 与非法句柄诊断 Soak

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0059: Nightly Sanitizer Gate 与非法句柄诊断 Soak.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

C.8 完成后，本地 `ASan/UBSan` 验证已覆盖 `security/soak_smoke/styio_pipeline/milestone`，但尚未进入可重复 CI。  
同时，非法句柄路径在 C.8 被定义为“非零句柄可诊断、close 兼容 no-op”，需要长跑回归保证该契约不会回退。

## Decision

1. 新增 nightly workflow：`.github/workflows/nightly-sanitizers.yml`。
2. workflow 使用 `clang-18` + `-fsanitize=address,undefined` 构建并执行：
   - `ctest -L security`
   - `ctest -L soak_smoke`
   - `ctest -L styio_pipeline`
   - `ctest -L milestone`
3. sanitizer 日志统一归档为 artifact（`nightly-sanitizer-artifacts`）。
4. Soak 新增 `StyioSoakSingleThread.InvalidHandleDiagnosticsLoop`：
   - 高频非法句柄 `rewind/read/write` 必须置 runtime error；
   - 非法句柄 `close` 保持兼容 no-op；
   - deep 档位注入 `STYIO_SOAK_INVALID_HANDLE_ITERS` 做长跑。

## Alternatives

1. 仅保留本地 sanitizer 验证，不进 CI。
2. 把 sanitizer 放进 PR 必跑流水线。
3. 不增加非法句柄长跑，只保留单元级断言。

## Consequences

1. sanitizer 验证从“人工执行”升级为“周期化自动门禁”，中断恢复时可直接读取 CI 结果。
2. 句柄诊断契约获得长生命周期回归覆盖，减少“改了错误路径后静默退化”的风险。
3. 维持 PR 时长：重型 sanitizer 仍在 nightly，日常 PR 不被明显拖慢。
