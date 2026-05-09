# Implemented Decisions Summary

**Purpose:** Compress implemented architecture and workflow decisions that no longer need one file per decision in the current tree; exact previous ADR wording is available from Git history when needed.

**Last updated:** 2026-05-09

## Reading Contract

1. This file is provenance, not an active planning queue.
2. Active rules live in the owning design, spec, workflow, team, test, or rollup document.
3. Add a new ADR file only while a decision still needs direct review as a decision record; after implementation and SSOT absorption, compress it here or remove it.

## Compressed Decision Groups

| Area | Implemented decision summary | Owning active docs |
|------|------------------------------|--------------------|
| Checkpoint delivery | Checkpoints, generated indexes, docs audit, lifecycle validation, team docs gates, and delivery gates are part of the normal delivery floor. | [workflow assets](../assets/workflow/INDEX.md), [team runbooks](../teams/INDEX.md), [current state](../rollups/CURRENT-STATE.md) |
| Diagnostics and runtime errors | CLI/runtime diagnostics use structured outputs, stable runtime error categories, last-error plumbing, and fail-closed behavior for unsupported active paths. | [agent spec](../specs/AGENT-SPEC.md), [test catalog](../assets/workflow/TEST-CATALOG.md), [codegen/runtime runbook](../teams/CODEGEN-RUNTIME-RUNBOOK.md) |
| AST ownership and memory | AST nodes, parser attachments, inline substitution, and clone paths use explicit ownership boundaries; unsupported ownership paths must be tested or rejected instead of silently borrowed. | [sema/IR runbook](../teams/SEMA-IR-RUNBOOK.md), [test catalog](../assets/workflow/TEST-CATALOG.md), [agent spec](../specs/AGENT-SPEC.md) |
| Parser migration | The compiler is nightly-first, with shadow gates, fallback metrics, internal bridge tracking, parser legacy-entry audit, and route statistics used to control migration risk. | [frontend runbook](../teams/FRONTEND-RUNBOOK.md), [test catalog](../assets/workflow/TEST-CATALOG.md), [checkpoint workflow](../assets/workflow/CHECKPOINT-WORKFLOW.md) |
| Five-layer pipeline | Lexer, IR, LLVM, runtime, and full-pipeline goldens are expected for accepted cross-layer behavior, including streams, file resources, stdin/stdout/stderr, and match-bind lowering. | [five-layer pipeline](../assets/workflow/FIVE-LAYER-PIPELINE.md), [test catalog](../assets/workflow/TEST-CATALOG.md) |
| Standard streams | `@stdin`, `@stdout`, and `@stderr` are canonical resource forms; stream reads and writes stay covered by accepted syntax, tests, and design references. | [active syntax](../design/syntax/ACTIVE-SYNTAX.md), [EBNF](../design/Styio-EBNF.md), [resource identifiers](../design/syntax/RESOURCE_IDENTIFIERS.md) |
| Resource topology | Resource topology uses compact source syntax backed by compiler-owned resource metadata, move/consume tracking, committed snapshot reads, `@()` as the destroy sink, and resource-method rules in the active topology design. | [resource topology](../design/Styio-Resource-Topology.md), [active syntax](../design/syntax/ACTIVE-SYNTAX.md), [next-stage ledger](../rollups/NEXT-STAGE-GAP-LEDGER.md) |
| IDE and LSP | IDE work uses stable identity, semantic database/query caches, incremental edits, workspace index discipline, LSP capability boundaries, and explicit latency/perf gates. | [IDE external docs](../external/for-ide/INDEX.md), [IDE/LSP runbook](../teams/IDE-LSP-RUNBOOK.md), [perf runbook](../teams/PERF-STABILITY-RUNBOOK.md) |
| Native extern and JIT | Native extern/JIT interop uses explicit compiler/runtime contracts and tests instead of undocumented host calls. | [agent spec](../specs/AGENT-SPEC.md), [codegen/runtime runbook](../teams/CODEGEN-RUNTIME-RUNBOOK.md) |
| Package handoff | `styio` owns compiler-side nano and compile-plan producer contracts; package-manager UX and registry lifecycle stay outside this repository. | [repository map](../specs/REPOSITORY-MAP.md), [Spio handoff](../external/for-spio/Styio-Nano-Spio-Coordination.md), [CLI contract matrix](../plans/Styio-Ecosystem-CLI-Contract-Matrix.md) |
