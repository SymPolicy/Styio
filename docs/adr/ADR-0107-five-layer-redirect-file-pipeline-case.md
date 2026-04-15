# ADR-0107: Five-Layer Pipeline 增加 Redirect/File Case

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0107: Five-Layer Pipeline 增加 Redirect/File Case.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-08

## Context

在 `ADR-0106` 之后，five-layer pipeline 已覆盖：

1. 基础表达式
2. 简单函数
3. 文件写入
4. 文件读取 iterator
5. snapshot/state
6. zip/file-stream
7. instant pull

但 `-> @file{...}` 这条 `resource.redirect` 路径仍只在里程碑测试里通过副作用观察，没有进入五层 golden。它与 `data >> @file{...}` 不同，当前 lowering 会把 `resource.redirect` 统一落成 `styio.ir.resource_write`，并带 `promote=1`。

## Decision

1. 新增 five-layer case：
   - `tests/pipeline_cases/p08_redirect_file/`
2. case 语义固定为：
   - `x = 42`
   - `x -> @file{"/tmp/styio_pipeline_redirect.txt"}`
3. 测试端在运行前清理输出文件，并在运行后断言 `/tmp/styio_pipeline_redirect.txt` 内容为 `42`。
4. golden 固定五层产物，覆盖：
   - `styio.ast.resource.redirect`
   - `styio.ir.resource_write ... promote=1`
   - `styio_file_open_write/styio_file_write_cstr` lowering

## Alternatives

1. 继续只靠 M5 端到端副作用测试：
   - 拒绝。无法看到 AST/IR/LLVM 层漂移。
2. 把 redirect 与普通 write 合并到同一个 five-layer case：
   - 拒绝。两条语义路径的 lowering 标志不同，合并后定位不清。
3. 优先补 `at-resource` 而不是 redirect：
   - 暂不采用。redirect 的风险更低、语义更独立，适合作为下一条五层落地。

## Consequences

1. `resource.redirect` 第一次进入 nightly-first five-layer golden。
2. 后续若 redirect 的 AST/IR/LLVM lowering 发生变化，回归会直接定位到具体层。
3. five-layer pipeline 对资源 I/O 的覆盖从“写入/读取”扩展到“redirect 语义”。
