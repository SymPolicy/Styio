# Sema / IR Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of AST lifecycle, semantic analysis, type inference, StyioIR lowering, string representation, and compilation session ownership.

**Last updated:** 2026-05-03

## Mission

Own the compiler middle layer from parsed AST to StyioIR and stable textual representation. This team protects AST ownership, type contracts, lowering shape, and reprs used by diagnostics and five-layer goldens. It does not own parser syntax or LLVM emission.

## Owned Surface

Primary paths:

1. `src/StyioAST/`
2. `src/StyioSema/`
3. `src/StyioLowering/`
4. `src/StyioIR/`
5. `src/StyioToString/`
6. `src/StyioSession/`

High-value docs:

1. [../design/Styio-Language-Design.md](../design/Styio-Language-Design.md)
2. [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md)
3. [../assets/workflow/FIVE-LAYER-PIPELINE.md](../assets/workflow/FIVE-LAYER-PIPELINE.md)

## Daily Workflow

1. Start from the language or capability SSOT for the feature.
2. Identify the AST node, type-inference rule, lowering rule, and IR node together before editing.
3. Keep `StyioSemaContext` responsible for type inference/state and `AstToStyioIRLowerer` responsible for AST-to-IR conversion; the legacy `StyioAnalyzer` name is compatibility only.
4. Keep ownership/view changes small and covered by safety or security tests.
5. Update five-layer goldens when AST or StyioIR textual shape intentionally changes.
6. Coordinate with Codegen / Runtime before changing IR consumed by LLVM emission.
7. Keep semantic lowering fail-closed: unknown user calls, user-call arity mismatches, unsupported AST nodes, and missing type slots must produce typed diagnostics or covered lowering rules, not `SGConstInt(0)` placeholders.
8. When adding or repairing AST nodes such as `SizeOf`, prove the full lifecycle: owned child expression, writable inferred type slot, typed inference result, and StyioIR lowering shape.
9. When parser syntax can represent one-shot continuations before lowering exists, emit explicit semantic errors instead of letting internal resume names leak as unknown user functions.
10. For terminal-handle and standard-stream iterable writes (`>> [>_]`, `>> @stdout`, `>> @stderr`), keep semantic checks stricter than `->`: require an iterable text-serializable type, reject scalar strings, and route explicit `string.lines()` through `list[string]` lowering.
11. Keep GenIR domain ownership physical as well as nominal: default/general nodes belong in `src/StyioIR/GenIR/SGIR.hpp`, IO/file/std-stream/network/filesystem nodes in `SIOIR.hpp`, and List/Dictionary/Matrix collection nodes in `SCIR.hpp`. `GenIR.hpp` is a compatibility aggregator, not the primary place for new nodes.
12. Normalize break depth at the AST and IR boundary. `BreakAST` and `SGBreak` may keep compatibility constructors, but their semantic depth is always 1 and lowering must not preserve historical multi-level counts.
13. Native `@extern` declarations must have a real middle-layer lifecycle: parse only top-level exported signatures, expose callable names to type inference, lower `@export` and `@extern` to explicit SG nodes, and reject unknown native calls before codegen. Do not inspect or reinterpret native function bodies.
14. `InfiniteLoopAST` must type-check its guard and body before lowering; conditional loop guards are bool-only, so non-bool values must fail in sema rather than reaching LLVM codegen.
15. Matrix typed literals and intrinsics must carry element kind and static shape through Sema into IR. Reject ragged rows, nonnumeric elements, add/sub shape mismatches, and invalid matmul dimensions before lowering; lower `m[row]`, `m[row][col]`, arithmetic operators, and `mat_*` intrinsics to explicit collection IR instead of placeholder constants.

## Change Classes

1. Small: local type rule, repr text fix, or non-contract helper cleanup. Run targeted unit and affected milestone tests.
2. Medium: AST node field, ownership, type inference, or lowering change. Add security or pipeline coverage and update goldens.
3. High: new semantic category, IR node family, session lifecycle rule, or capability/failure model change. Use checkpoint workflow and add ADR if lifecycle or compatibility changes.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build/default -L milestone
ctest --test-dir build/default -L styio_pipeline
ctest --test-dir build/default -L security
```

When AST or IR text changes:

```bash
STYIO_PIPELINE_DUMP_FULL=1 ctest --test-dir build/default -L styio_pipeline --output-on-failure
```

For checkpoint-grade validation:

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

## Cross-Team Dependencies

1. Frontend must review changes that require new AST construction or parser recovery behavior.
2. Codegen / Runtime must review IR shape or type changes consumed by LLVM lowering.
3. Test Quality must review five-layer, semantic failure, and security coverage.
4. Docs / Ecosystem must review capability or design SSOT updates.

## Handoff / Recovery

Record unfinished middle-layer work with:

1. AST nodes and ownership state.
2. Type-inference and lowering rule status.
3. Expected repr or IR text delta.
4. Failing five-layer layer, if any.
5. Whether Codegen has already been adapted.
6. Remaining unsupported-lowering handlers and the negative matrix needed to retire placeholder fallback safely.
