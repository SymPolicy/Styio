# Add Syntax With Skills

**Purpose:** Orchestrate new Styio syntax work through repo-local skills, implementation surfaces, docs, and gates so parser acceptance, IR lowering, runtime helper exports, ORC JIT registration, and test/docs gates land as one checkpoint.

**Last updated:** 2026-05-22

**TOML:** [ADD-SYNTAX-WITH-SKILLS.toml](./ADD-SYNTAX-WITH-SKILLS.toml) is the machine-readable workflow definition.

## Skill

Use [styio-syntax-change/skill.toml](./skills/styio-syntax-change/skill.toml) when the request touches syntax, operators, tokens, grammar, parser behavior, IDE highlighting, semantics, or syntax docs/tests.

## Trigger

Run this workflow whenever a change does at least one of the following:

1. adds or removes accepted syntax;
2. changes parser routing, AST shape, or recovery behavior for accepted syntax;
3. adds or renames a `styio_*` runtime helper used by lowering or execution;
4. changes how accepted syntax reaches JIT-executed runtime behavior.

## Workflow

1. Load the skill and its surface map.
2. Freeze the accepted examples and reserved symbols.
3. Implement in this order:
   - token enum/name/lexer
   - IDE tokenizer
   - authoritative nightly parser
   - legacy parser migration evidence only when the change touches retired syntax
   - AST/type/IR behavior
   - docs and runbooks
   - tests
4. If runtime lowering is incomplete, reject the accepted-looking form with a typed fail-closed diagnostic.
5. Prove no-fallback acceptance and boundaries before reporting completion.

## Mandatory Flow

| Step | Owner | Required Surface | Evidence | Boundary |
|------|-------|------------------|----------|----------|
| 1 | Docs / Language owner | `docs/design/Styio-Language-Design.md`, `docs/design/Styio-EBNF.md`, `docs/design/Styio-Symbol-Reference.md` | Language SSOT diff | Defines accepted syntax only; does not redefine tests or runtime policy. |
| 2 | Frontend | `src/StyioToken/`, `src/StyioParser/`, parser fixtures | Parser or feature regression | Implements acceptance only; does not encode semantic/runtime ownership. |
| 3 | Sema / IR | `src/StyioAST/`, `src/StyioSema/`, `src/StyioLowering/`, `src/StyioIR/` | IR or sema/lowering test evidence | Defines meaning and IR shape; does not own LLVM helper registration. |
| 4 | Codegen / Runtime | `src/StyioCodeGen/`, `src/StyioExtern/ExternLib.hpp`, `src/StyioExtern/ExternLib.cpp`, `src/StyioJIT/StyioJIT_ORC.hpp` | `python3 scripts/runtime-surface-gate.py` | Keeps helper calls, exports, implementations, and ORC registrations aligned. |
| 5 | Test Quality | `tests/`, `workflows/TEST-CATALOG.md`, five-layer goldens | `ctest` label or parser shadow evidence | Records behavior evidence; does not redefine language semantics. |
| 6 | Docs / Ecosystem | Workflow/runbook docs and generated indexes | `python3 scripts/workflow-scheduler.py check` and docs gates | Keeps workflow discoverability and ownership boundaries current. |
| 7 | Delivery owner | Scheduler profile and delivery floor | `python3 scripts/workflow-scheduler.py run --profile syntax-local` or `./scripts/delivery-gate.sh` | Executes the registered chain; does not introduce ad hoc gate order. |

## Required Evidence

1. Lexer coverage for every new token.
2. Authoritative nightly parser coverage for every accepted form, with no accepted-grammar fallback.
3. Semantic negative coverage for every accepted-but-not-lowered form.
4. Runtime smoke for every supported lowering path.
5. Docs updated in compact syntax, EBNF, symbol reference, and semantic SSOT.

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

Focused build and feature gates:

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -L security --output-on-failure
ctest --test-dir build/default -R '^StyioParserEngine\.' --output-on-failure
ctest --test-dir build/default -R '^parser_shadow_gate_' --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
git diff --check
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

## Handoff

Report accepted syntax, reserved syntax, unsupported lowering paths, docs touched, exact gates run, and remaining lifecycle or runtime work.
