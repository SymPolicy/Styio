# Syntax Addition Workflow

**Purpose:** Define the mandatory delivery workflow for adding or changing Styio syntax so parser acceptance, IR lowering, runtime helper exports, ORC JIT registration, and test/docs gates land as one checkpoint.

**Last updated:** 2026-04-26

## Trigger

Run this workflow whenever a change does at least one of the following:

1. adds or removes accepted syntax;
2. changes parser routing, AST shape, or recovery behavior for accepted syntax;
3. adds or renames a `styio_*` runtime helper used by lowering or execution;
4. changes how accepted syntax reaches JIT-executed runtime behavior.

## Mandatory Flow

1. Update the language SSOT first:
   - `docs/design/Styio-Language-Design.md`
   - `docs/design/Styio-EBNF.md`
   - `docs/design/Styio-Symbol-Reference.md`
2. Update the frontend surface in the same checkpoint:
   - `src/StyioToken/`
   - `src/StyioParser/`
   - parser-facing fixtures and milestone coverage
3. If AST or semantic meaning changes, update:
   - `src/StyioAST/`
   - `src/StyioAnalyzer/`
   - `src/StyioIR/`
4. If lowering adds or renames runtime helper calls, update all three runtime surfaces together:
   - codegen call sites under `src/StyioCodeGen/`
   - export declarations/definitions in `src/StyioExtern/ExternLib.hpp` and `src/StyioExtern/ExternLib.cpp`
   - ORC symbol registration in `src/StyioJIT/StyioJIT_ORC.hpp`
5. Add or refresh tests for the accepted behavior:
   - milestone / parser fixtures
   - five-layer pipeline coverage when lowering or runtime behavior changes
   - security / soak coverage when runtime contracts change
6. Run the runtime registration gate before delivery:

```bash
python3 scripts/runtime-surface-gate.py
```

7. Run the common delivery floor:

```bash
./scripts/delivery-gate.sh --mode checkpoint
```

8. Update owner runbooks and workflow docs in the same delivery whenever the syntax change introduces a new delivery requirement or runtime-registration rule.

## Hard Blockers

Do not merge a syntax change when any of the following is true:

1. parser acceptance changed but the language SSOT did not;
2. codegen emits a new `styio_*` helper but `ExternLib.hpp` or `ExternLib.cpp` was not updated;
3. a runtime helper is exported but missing from `StyioJIT_ORC.hpp`;
4. the syntax change only updated parser tests and skipped pipeline/runtime coverage;
5. workflow docs changed without corresponding automation updates.

## Definition Of Done

A syntax delivery is complete only when:

1. the accepted syntax is documented in the language SSOT;
2. parser and lowering behavior are covered by tests;
3. runtime helper exports, implementations, and ORC registrations are aligned;
4. `python3 scripts/runtime-surface-gate.py` passes;
5. the common delivery floor passes.
