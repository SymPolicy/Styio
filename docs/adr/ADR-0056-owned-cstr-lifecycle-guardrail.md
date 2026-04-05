# ADR-0056: `styio_strcat_ab` 所有权追踪与安全释放护栏

- **Status:** Accepted
- **Date:** 2026-04-05

## Context

`styio_strcat_ab` 返回堆内存字符串，历史路径中存在两类风险：

1. codegen 未对拼接临时值建立稳定释放路径，长链拼接在长生命周期程序中会累积泄漏。
2. `styio_free_cstr` 直接 `free` 输入指针，若误传借用指针（如 `styio_file_read_line` 返回值）会触发未定义行为风险。

Milestone C.7 需要先完成“显式释放路径 + 上层可控处理”的第一阶段收口。

## Decision

1. 在 runtime 层为 `styio_strcat_ab` 分配结果建立线程本地所有权登记表：
   - `styio_strcat_ab` 成功分配后登记指针；
   - `styio_free_cstr` 仅释放登记过的指针，并在释放时注销。
2. `styio_free_cstr` 对 `nullptr` 和非登记指针保持安全 no-op，避免误释放借用指针。
3. 在 codegen 层增加“临时拥有字符串”追踪集：
   - `SGBinOp` 字符串 `+` 产出追踪临时值；
   - 当临时值被下一次拼接消费时，立即回收旧临时值；
   - `SIOPrint` 与 `SGResourceWriteToFile` 在消费后回收追踪临时值。
4. 在绑定路径增加局部 RAII：
   - 新声明的字符串槽位登记到 scope，`pop_file_handle_scope` 时统一回收当前值；
   - 赋值覆写阶段暂不做“立即释放旧值”，先避免别名场景的潜在 UAF；
   - `SGFallback`、`SGMatch`、`SGWaveMerge`、`SGGuardSelect` 在 pointer 合并时转移临时值追踪，避免在 PHI/select 链路丢失释放机会。
5. 安全回归同步更新：
   - `StrcatAbAllocatesAndSupportsPairingFree`；
   - `FreeCstrIgnoresBorrowedPointer`。

## Alternatives

1. 引入全局 GC/引用计数字符串对象，替代 `const char*` ABI。
2. 改为 arena 分配并在进程结束统一释放，不做中途回收。
3. 保持现状，仅在测试中手工 `free`。

## Consequences

1. 临时拼接值在“拼接-消费”链路上具备可验证回收路径，泄漏面显著收敛。
2. runtime 对误用 `styio_free_cstr` 的鲁棒性提升，借用指针误传不再导致直接崩溃。
3. 该方案保持现有 C ABI，不引入外部行为破坏。
4. 仍存在边界：含提前终止控制流的极端路径下，局部 scope 回收可能不完整；赋值覆写未做即时释放。下一步通过 ASan/soak 压测与别名策略补洞（C.7.2）。
