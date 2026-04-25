# Sema / IR Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of AST lifecycle, semantic analysis, type inference, StyioIR lowering, string representation, and compilation session ownership.

**Last updated:** 2026-04-15

## Mission

Own the compiler middle layer from parsed AST to StyioIR and stable textual representation. This team protects AST ownership, type contracts, lowering shape, and reprs used by diagnostics and five-layer goldens. It does not own parser syntax or LLVM emission.

## Owned Surface

Primary paths:

1. `src/StyioAST/`
2. `src/StyioAnalyzer/`
3. `src/StyioIR/`
4. `src/StyioToString/`
5. `src/StyioSession/`

High-value docs:

1. [../design/Styio-Language-Design.md](../design/Styio-Language-Design.md)
2. [../design/Styio-Handle-Capability-Type-System.md](../design/Styio-Handle-Capability-Type-System.md)
3. [../assets/workflow/FIVE-LAYER-PIPELINE.md](../assets/workflow/FIVE-LAYER-PIPELINE.md)

## Daily Workflow

1. Start from the language or capability SSOT for the feature.
2. Identify the AST node, type-inference rule, lowering rule, and IR node together before editing.
3. Keep ownership/view changes small and covered by safety or security tests.
4. Update five-layer goldens when AST or StyioIR textual shape intentionally changes.
5. Coordinate with Codegen / Runtime before changing IR consumed by LLVM emission.

## Change Classes

1. Small: local type rule, repr text fix, or non-contract helper cleanup. Run targeted unit and affected milestone tests.
2. Medium: AST node field, ownership, type inference, or lowering change. Add security or pipeline coverage and update goldens.
3. High: new semantic category, IR node family, session lifecycle rule, or capability/failure model change. Use checkpoint workflow and add ADR if lifecycle or compatibility changes.

## Required Gates

Minimum local commands:

```bash
ctest --test-dir build -L milestone
ctest --test-dir build -L styio_pipeline
ctest --test-dir build -L security
```

When AST or IR text changes:

```bash
STYIO_PIPELINE_DUMP_FULL=1 ctest --test-dir build -L styio_pipeline --output-on-failure
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
