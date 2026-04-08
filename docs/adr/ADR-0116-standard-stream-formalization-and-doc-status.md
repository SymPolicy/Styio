# ADR-0116: Standard Stream Formalization, `SIOStd*` Naming, and Doc Status Boundaries

**Status:** Accepted

## Context

标准流（stdin/stdout/stderr）功能已经在代码、里程碑 fixture、five-layer pipeline 和多份文档里落地，但出现了三类漂移：

1. IR 命名上仍混有早期 `SG*` 语义，和用户约定的 `SIOStd*` 家族不一致；
2. 2026-04-08 冻结文档把 `-> @stdout/@stderr` 写成唯一合法形式，但实际实现与 five-layer case 已长期接受 `>> @stdout/@stderr`；
3. 2026-03-29 的 M9/M10 草案和 2026-04-08 的冻结规格同时存在，却没有显式标注 superseded，破坏 checkpoint 冷启动时的恢复可读性。

如果不收口这三点，后续 parser / docs / recovery gate 会继续在“canonical 写法”和“兼容表面”之间来回漂移。

## Decision

1. 标准流专用 IR 家族统一使用 `SIOStd*` 命名：
   - `SIOStdStreamWrite`
   - `SIOStdStreamLineIter`
   - `SIOStdStreamPull`
2. 当前冻结实现把标准流定义为**编译器直接识别的资源 atom**：
   - `@stdout`
   - `@stderr`
   - `@stdin`
   - 不要求也不依赖用户在源码中先写 `@stdout := ...` 之类的 wrapper 定义
3. 标准流写语法分为两层：
   - **canonical**：`expr -> @stdout` / `expr -> @stderr`
   - **accepted compatibility shorthand**：`expr >> @stdout` / `expr >> @stderr`
4. `expr >> @stdout/@stderr` 的行为被正式定义为：
   - 当 `>>` 后面是标准流资源 atom 时，parser 产生 `ResourceWriteAST`
   - lowering 与 canonical `-> @stdout/@stderr` 进入同一 `SIOStd*` IR 路径
5. `@stdin` 保持严格 read-only：
   - `expr -> @stdin`
   - `expr >> @stdin`
   两者都属于语义错误。
6. 文档层明确分层：
   - `docs/plans/stdin-stdout-stderr.md`：历史计划，不是 SSOT
   - `docs/milestones/2026-04-08/`：冻结规格
   - `docs/milestones/2026-03-29/M9-StdoutStderr.md` 与 `M10-StdinValidation.md`：标记为 `Superseded draft`
7. `StyioRepr` 对 stdout 路径继续保留 `styio.ir.print` 文本契约，以维持既有 five-layer goldens；类名统一为 `SIOStd*` 不强制要求 debug 字符串同名。

## Alternatives

1. 直接废掉 `>> @stdout/@stderr`
   - 问题：现有 five-layer cases、用户样例和 parser 行为已经依赖这层兼容语法，直接移除会制造不必要破坏面。
2. 反过来把 `>> @stdout/@stderr` 提升为新的 canonical
   - 问题：冻结里程碑和教学文档已经稳定采用 `-> @stdout/@stderr`，而 `->` 对“重定向到 sink”语义也更直观。
3. 不区分 frozen/superseded 文档，只在最新文档里悄悄修正
   - 问题：历史草案仍会继续误导后续恢复和 review。

## Consequences

1. 后续任何标准流相关文档必须显式区分 canonical 与 accepted compatibility shorthand。
2. parser / five-layer / safety regression 需要继续保留至少一条 `>> @stdout/@stderr` 自动化覆盖。
3. 标准流相关类名、visitor 声明、codegen/type hooks 全部以 `SIOStd*` 为准。
4. 旧草案不会被删除，但必须在文首明确“已被冻结规格取代”。
