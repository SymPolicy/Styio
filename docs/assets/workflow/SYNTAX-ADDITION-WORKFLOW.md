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

| Step | Owner | Required Surface | Evidence | Boundary |
|------|-------|------------------|----------|----------|
| 1 | Docs / Language owner | `docs/design/Styio-Language-Design.md`, `docs/design/Styio-EBNF.md`, `docs/design/Styio-Symbol-Reference.md` | Language SSOT diff | Defines accepted syntax only; does not redefine tests or runtime policy. |
| 2 | Frontend | `src/StyioToken/`, `src/StyioParser/`, parser fixtures | Parser or milestone regression | Implements acceptance only; does not encode semantic/runtime ownership. |
| 3 | Sema / IR | `src/StyioAST/`, `src/StyioAnalyzer/`, `src/StyioIR/` | IR or analyzer test evidence | Defines meaning and IR shape; does not own LLVM helper registration. |
| 4 | Codegen / Runtime | `src/StyioCodeGen/`, `src/StyioExtern/ExternLib.hpp`, `src/StyioExtern/ExternLib.cpp`, `src/StyioJIT/StyioJIT_ORC.hpp` | `python3 scripts/runtime-surface-gate.py` | Keeps helper calls, exports, implementations, and ORC registrations aligned. |
| 5 | Test Quality | `tests/`, `docs/assets/workflow/TEST-CATALOG.md`, five-layer goldens | `ctest` label or parser shadow evidence | Records behavior evidence; does not redefine language semantics. |
| 6 | Docs / Ecosystem | Workflow/runbook docs and generated indexes | `python3 scripts/workflow-scheduler.py check` and docs gates | Keeps workflow discoverability and ownership boundaries current. |
| 7 | Delivery owner | Scheduler profile and delivery floor | `python3 scripts/workflow-scheduler.py run --profile syntax-local` or `./scripts/delivery-gate.sh` | Executes the registered chain; does not introduce ad hoc gate order. |

## Required Gates

Run the runtime registration gate before delivery:

```bash
python3 scripts/runtime-surface-gate.py
```

Run the workflow scheduler check so new workflow docs and tools cannot bypass registration:

```bash
python3 scripts/workflow-scheduler.py check
python3 tests/workflow_scheduler_test.py
```

Run the local syntax profile:

```bash
python3 scripts/workflow-scheduler.py run --profile syntax-local
```

Run the common delivery floor:

```bash
./scripts/delivery-gate.sh
```

Update owner runbooks and workflow docs in the same delivery whenever the syntax change introduces a new delivery requirement or runtime-registration rule.

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
