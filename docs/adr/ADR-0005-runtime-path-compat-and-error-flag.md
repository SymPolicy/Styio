# ADR-0005: 运行时文件路径兼容与错误旗标

- **Status:** Accepted
- **Date:** 2026-04-03

## Context

在里程碑回归中发现两类稳定性问题：

1. 历史测试样例仍引用 `tests/m5/...`、`tests/m7/...`，但仓库目录已迁移到 `tests/milestones/...`。
2. Runtime C ABI 已从 `exit(1)` 迁移为返回错误值，导致“打开文件失败”场景可能静默返回成功退出码（不满足 fail-fast 用例）。

此外，`match` 混合返回（string + int）时，整数转字符串曾返回栈内临时缓冲区指针，造成悬垂指针与乱码输出。

## Decision

1. 在 runtime 文件读取路径增加兼容映射：当输入路径不存在且匹配 `tests/m<digit>/...` 时，自动尝试 `tests/milestones/m<digit>/...`。
2. 在 runtime 引入线程局部错误旗标（`styio_runtime_has_error` / `styio_runtime_clear_error`），`main` 在 JIT 执行后统一检查并转为稳定非零退出码。
3. 混合表达式字符串提升不再返回栈缓冲区指针；整数/浮点统一改为调用 runtime 的线程局部字符串转换函数（`styio_i64_dec_cstr` / `styio_f64_dec_cstr`）。

## Alternatives

1. 修改全部历史测试样例路径并删除兼容层。
2. 恢复 runtime 内部 `exit(1)` 终止策略。
3. 继续使用 `snprintf + alloca` 栈缓冲区并依赖调用方立即消费。

## Consequences

1. 历史路径迁移不再阻塞回归，旧样例可直接运行。
2. 保持“runtime 不直接 exit”策略，同时恢复 fail-fast 的可观测退出码。
3. 消除 mixed-match 返回值的悬垂指针风险，修复 `fizzbuzz` 乱码问题。
