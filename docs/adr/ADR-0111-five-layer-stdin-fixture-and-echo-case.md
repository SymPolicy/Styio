# ADR-0111: Five-Layer Pipeline 增加 Stdin Fixture 与 Echo Case

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0111: Five-Layer Pipeline 增加 Stdin Fixture 与 Echo Case.

**Last updated:** 2026-04-08

- Status: Accepted
- Date: 2026-04-08

## Context

在 `ADR-0110` 之后，five-layer pipeline 已覆盖大量文件与流处理路径，但 L5 子进程仍只能跑“纯源码 + 无 stdin”的程序。M10 的 `@stdin` 路径因此只能停留在里程碑端到端测试里，无法进入 AST/IR/LLVM/stdout 五层 golden。

同时，`name >> @resource` 在 parser 里存在一个 name-led 分流缺口：它会优先掉进 iterator 路径，导致 `line >> @stdout` 被误解析并在运行时把 `line` 当成文件句柄使用。

## Decision

1. `src/StyioTesting/PipelineCheck.cpp` 的 L5 子进程执行增加可选 `stdin.txt` fixture：
   - 若 case 根目录存在 `stdin.txt`，则以 shell 重定向方式喂给 `styio --file input.styio`。
2. 修正 parser 的 name-led `>>` 分流：
   - `name >> @resource` 优先解析为 `ResourceWriteAST`
   - `name >> #(x) => { ... }` 仍保持 iterator 语义
3. 新增 five-layer case：
   - `tests/pipeline_cases/p12_stdin_echo/`
4. case 语义固定为：
   - `@stdin >> #(line) => { line >> @stdout }`
   - `stdin.txt` 内容为 `hello/world`

## Alternatives

1. 继续只靠 M10 端到端 stdout 比对：
   - 拒绝。无法冻结 AST/IR/LLVM 层行为。
2. 不改 PipelineCheck，只在测试里拼接 shell 命令给 stdin：
   - 拒绝。会把 fixture 规则散落到单个测试里，不利于恢复。
3. 先做完整 stdin/stderr 双流框架再加 case：
   - 暂不采用。当前更重要的是先让 `stdin` 能进入 five-layer 基线。

## Consequences

1. five-layer pipeline 第一次支持带 stdin fixture 的 case。
2. `@stdin >> #(line)` 与 `line >> @stdout` 的组合路径第一次进入 nightly-first golden。
3. `stdin.txt` 成为后续 M10 five-layer cases 的统一输入约定。
