# ADR-0117: Standard Stream Milestone Contract Audit

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0117: Standard Stream Milestone Contract Audit.

**Last updated:** 2026-04-08

## Status

Accepted

## Context

在标准流（M9/M10）推进过程中，代码、five-layer golden、里程碑 fixture 与冻结文档之间出现了两类漂移：

1. `M9` 把 `$"..."` format string 放进了 stdout 里程碑验收，但当前 format string 仍是独立的部分实现特性，不应该反向绑死标准流里程碑。
2. `M10` 的 `(<< @stdin)` 在真实实现中沿用了既有 `InstantPullAST` 标量约定，LLVM lowering 固定经过 `styio_cstr_to_i64()`；但冻结文档和部分 milestone fixture 却把它写成了“原样字符串回显”。
3. 部分任务说明把 `styio_stdin_read_line()` 记成 “malloc 返回 + 调用方 free”，与实际 thread-local borrowed buffer 契约不一致。

如果不先做这次契约审计，后续 checkpoint 会继续拿错误的文档目标去修代码，导致分支腐化和错误回归。

## Decision

1. `M9` 冻结验收不再依赖 format string。
   - `T9.06` 改为普通字符串拼接输出。
   - format string 保持为单独特性轨道，不作为标准流里程碑完成条件。
2. `M10` 冻结 `(<< @stdin)` 的当前实现契约：
   - line-iterator: 行值按字符串路径工作；
   - instant pull: 当前保持 `cstr -> i64` 标量 lowering 约定。
3. `M10` 的 numeric instant-pull 契约要在三层同时写清：
   - milestone acceptance
   - five-layer/test catalog
   - history/ADR
4. `styio_stdin_read_line()` 的 runtime 契约写实：
   - thread-local borrowed buffer
   - 不由调用方 free
5. 2026-04-08 的 stdio 文档继续作为 frozen batch，但需要明确：
   - no wrapper definitions
   - canonical `-> @stdout/@stderr`
   - accepted shorthand `>> @stdout/@stderr`
   - instant pull 的当前 numeric contract

## Alternatives

1. 直接把 `(<< @stdin)` 改成字符串语义，再让里程碑文档保持不动。
   - 问题：会直接打破已冻结的 `p14_stdin_pull` five-layer golden 和现有 lowering 形状，不适合在这次“文档/契约收口” checkpoint 里混做。
2. 保持文档不改，只修测试输入直到绿。
   - 问题：会继续让冻结文档对实现撒谎，后续恢复无法判断哪个才是 SSOT。
3. 继续要求 M9 覆盖 format string。
   - 问题：把两个未同时收口的特性绑成一个里程碑，违反 checkpoint 微型化原则。

## Consequences

正面：

1. M9/M10 的验收目标重新与真实实现对齐。
2. five-layer、milestone、ADR、history 对 `(<< @stdin)` 的契约描述一致。
3. format string 不再作为标准流里程碑的隐性阻塞项。

负面：

1. `M10` 当前冻结的是 numeric instant-pull，而不是更直觉的 string instant-pull；未来如果切换到 typed string path，需要单独开 checkpoint 并更新 goldens。
2. `T9.06` 不再验证 `$"..."`，format string 需要继续由其自身路线收口。
