# ADR-0058: 运行时句柄表真实接管与 `0` 句柄兼容边界

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0058: 运行时句柄表真实接管与 `0` 句柄兼容边界.

**Last updated:** 2026-04-08

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

C.8 目标要求将 runtime C ABI 的文件资源从“裸整型 + 外部 map 约定”推进为受控句柄表，并保证：

1. 非法句柄不会导致进程崩溃；
2. 错误可诊断（stderr + runtime error flag）；
3. `close` 保持幂等；
4. 不破坏既有流式执行路径。

在接管过程中发现现有 IR 里存在以 `0` 作为“非文件流哨兵”的路径。如果将 `0` 一律视为非法句柄并置错，会导致 `# ... >> ...` 等链路出现非预期 `RuntimeError`（退出码 `5`）。

## Decision

1. `styio_file_open/open_auto/open_write` 返回的文件句柄统一由线程本地 `StyioHandleTable` 分配与追踪（`HandleKind::File`）。
2. `styio_file_close` 通过句柄表释放并执行 `fclose`，保持幂等：
   - 已关闭/未知句柄不崩溃；
   - 重复 close 不置 runtime error。
3. `styio_file_read_line` / `styio_file_rewind` / `styio_file_write_cstr` 对“非零非法句柄”执行稳定诊断：
   - 打印 `styio: invalid file handle: <id>` 到 `stderr`；
   - 置 `g_runtime_error = true`。
4. `0` 句柄保留为兼容哨兵：
   - 直接返回空/不执行操作；
   - 不置 runtime error。
5. 增加线程退出清理：句柄表析构前调用 `release_all(File, fclose)`，回收遗漏文件资源。
6. 增加 Safety 回归：
   - 运行时：缺失文件、非法读写/rewind 置错、close 幂等；
   - 句柄表：类型匹配、release/release_all、stub 清理。

## Alternatives

1. 继续使用 `unordered_map<int64_t, FILE*>` + 手写序号。
2. 将 `0` 句柄也视为错误并统一置错。
3. 把非法句柄全部静默 no-op，不提供诊断。

## Consequences

1. 文件资源生命周期进入统一受控路径，减少“关闭后悬挂句柄/误用类型”风险。
2. 非零非法句柄在运行期可观测，便于回归与现场定位。
3. 保留 `0` 哨兵兼容后，既有流式语义与 parser shadow/pipeline 基线不被破坏。
4. C.8 验收从“结构存在”提升到“行为稳定 + 可诊断 + 可回归”。
