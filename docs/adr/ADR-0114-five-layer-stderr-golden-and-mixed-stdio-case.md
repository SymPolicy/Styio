# ADR-0114: Five-Layer Pipeline 增加 Stderr Golden 与 Mixed Stdio Case

**Purpose:** Record the decision, context, alternatives, and consequences for ADR-0114: Five-Layer Pipeline 增加 Stderr Golden 与 Mixed Stdio Case.

**Last updated:** 2026-04-08

## Context

在 `ADR-0111`、`ADR-0112`、`ADR-0113` 之后，five-layer pipeline 已覆盖 `@stdin` 的 echo、transform 和 instant pull，但 L5 仍只比较 `stdout.txt`。

这让 `M10` 的 `t06_stdin_mixed_output` 无法进入 five-layer，因为它需要同时约束：

1. `stdout` 上的 `got: data`
2. `stderr` 上的 `processing: data`

与此同时，`t06` 还暴露了一个真实 parser 语义缺口：

```styio
"got: " + line >> @stdout
```

原先会被解析成：

```styio
"got: " + (line >> @stdout)
```

导致前缀字符串丢失，运行时只输出 `data`。

## Decision

1. 在 `Parser.cpp` 中对 `Binary_Add` + `ResourceWriteAST/ResourceRedirectAST` 做局部重关联：
   - 将 `lhs + (rhs >> @stream)` 重写为 `(lhs + rhs) >> @stream`
   - 仅作用于 `+` 与资源写出/redirect 组合，避免扩大 parser 影响面
2. 为 `ResourceWriteAST` / `ResourceRedirectAST` 增加释放子节点的接口，以支持安全重组 AST 所有权。
3. 将 `src/StyioTesting/PipelineCheck.cpp` 的 L5 扩展为：
   - 始终比较 `expected/stdout.txt`
   - 当 `expected/stderr.txt` 存在时，额外比较 stderr
4. 新增 five-layer case `tests/pipeline_cases/p15_stdin_mixed_output/`，冻结：
   - typed AST
   - StyioIR
   - LLVM IR
   - stdout `got: data`
   - stderr `processing: data`
5. 继续保留 `tests/milestones/m10/t06_stdin_mixed_output.styio` 作为端到端里程碑回归。

## Consequences

正面：

1. `M10` 的 mixed stdout/stderr 路径不再只靠里程碑 shell test，而是进入 five-layer golden。
2. `+` 与资源写出的组合语义现在与里程碑文档一致，`t06` 从失败转绿。
3. five-layer L5 不再局限于 stdout，后续可以继续覆盖 stderr-sensitive runtime 行为。

负面：

1. parser 的这次修正是一个局部语义重关联；如果未来要系统化重写优先级规则，需要重新审视这段逻辑是否仍应保留。
2. `stderr.txt` 作为可选 golden 增加了用例维护面，新增 mixed-stdio case 时需要同步维护两份运行输出。
