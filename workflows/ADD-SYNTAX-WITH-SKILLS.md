# Add Syntax With Skills

**Purpose:** Orchestrate syntax work from a distributed feature SSOT through dependency/readiness resolution, language-owner decision, implementation, runtime registration, and convergence evidence.

**Last updated:** 2026-07-30

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
2. Create or update exactly one feature SSOT under `docs/design/syntax/features/`.
3. Freeze canonical, reserved, and rejected examples in that feature document.
4. Declare every feature dependency, prerequisite-evidence path, document role, owner, and current decision/delivery state.
5. Run `python3 scripts/syntax-feature-state-gate.py --write`, then run the gate in check mode.
6. Obtain the language-owner resolution required by `docs/specs/AGENT-SPEC.md`.
7. Begin implementation only when the feature decision is `accepted` and derived readiness is `ready`.
8. Implement in this order:
   - token enum/name/lexer
   - IDE tokenizer
   - authoritative nightly parser
   - retired parser-route audit evidence only when the change touches retired syntax
   - AST/type/IR behavior
   - tests
   - feature evidence and affected composed views/runbooks
9. If runtime lowering is incomplete, keep delivery below `converged` and reject the accepted-looking form with a typed fail-closed diagnostic.
10. Prove no-fallback acceptance, dependency closure, runtime behavior, and document-graph freshness before reporting completion.

## Mandatory Flow

| Step | Owner | Required Surface | Evidence | Boundary |
|------|-------|------------------|----------|----------|
| 1 | Feature / Language owner | `docs/design/syntax/features/<feature-id>.md` | Feature contract, decision, dependencies, prerequisites, and document roles | Owns one feature's long-lived SSOT; does not duplicate unrelated language features. |
| 2 | Document graph | Feature SSOT collection and `SYNTAX-FEATURE-GRAPH.json` | `python3 scripts/syntax-feature-state-gate.py` | Derives readiness and generated views; never becomes a competing authored SSOT. |
| 3 | Shared language contracts | `docs/design/Styio-Language-Design.md`, `docs/design/Styio-EBNF.md`, `docs/design/Styio-Symbol-Reference.md` | Cross-feature invariant diff or feature-owned references | Owns shared invariants only; feature-specific evolution starts from the feature SSOT. |
| 4 | Frontend | `src/StyioToken/`, `src/StyioParser/`, parser fixtures | Parser or feature regression | Implements acceptance only; does not encode semantic/runtime ownership. |
| 5 | Sema / IR | `src/StyioAST/`, `src/StyioSema/`, `src/StyioLowering/`, `src/StyioIR/` | IR or sema/lowering test evidence | Implements feature meaning and IR shape; does not own LLVM helper registration. |
| 6 | Codegen / Runtime | `src/StyioCodeGen/`, `src/StyioExtern/ExternLib.hpp`, `src/StyioExtern/ExternLib.cpp`, `src/StyioJIT/StyioJIT_ORC.hpp` | `python3 scripts/runtime-surface-gate.py` | Keeps helper calls, exports, implementations, and ORC registrations aligned. |
| 7 | Test Quality | `tests/`, `workflows/TEST-CATALOG.md`, five-layer goldens | `ctest` label or parser shadow evidence | Records behavior evidence; does not redefine language semantics. |
| 8 | Docs / Ecosystem | Workflow/runbook docs and generated indexes | scheduler and docs gates | Keeps composed views, workflow discoverability, and ownership boundaries current. |
| 9 | Delivery owner | Scheduler profile and delivery floor | syntax-local profile or delivery gate | Executes the registered chain; does not introduce ad hoc gate order. |

## Required Evidence

1. One feature SSOT with complete document roles, dependencies, prerequisites, and owner resolution.
2. A fresh generated feature graph with derived readiness `ready` before implementation advances.
3. Lexer coverage for every new token.
4. Authoritative nightly parser coverage for every accepted form, with no accepted-grammar fallback.
5. Semantic negative coverage for every accepted-but-not-lowered form.
6. Runtime smoke for every supported lowering path.
7. Feature evidence plus affected shared contracts and composed views updated without duplicating the feature SSOT.

## Required Gates

Run the runtime registration gate before delivery:

```bash
python3 scripts/syntax-feature-state-gate.py
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
python3 tests/syntax_feature_state_gate_test.py
python3 scripts/docs-index.py --check
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
git diff --check
```

Update owner runbooks and workflow docs in the same delivery whenever the syntax change introduces a new delivery requirement or runtime-registration rule.

## Hard Blockers

Do not merge a syntax change when any of the following is true:

1. no owning feature SSOT exists;
2. decision is not `accepted`, or readiness is not `ready`, when implementation advances;
3. a dependency, prerequisite, document role, implementation symbol, or golden case is unresolved;
4. parser acceptance changed but the owning feature SSOT did not;
5. codegen emits a new `styio_*` helper but `ExternLib.hpp` or `ExternLib.cpp` was not updated;
6. a runtime helper is exported but missing from `StyioJIT_ORC.hpp`;
7. the syntax change only updated parser tests and skipped pipeline/runtime coverage;
8. workflow docs changed without corresponding automation updates.

## Definition Of Done

A syntax delivery is complete only when:

1. the feature SSOT records the accepted syntax and its complete dependency/prerequisite closure;
2. decision is `accepted`, delivery is `converged`, and derived readiness is `ready`;
3. parser and lowering behavior are covered by tests;
4. runtime helper exports, implementations, and ORC registrations are aligned;
5. syntax-feature state and runtime-surface gates pass;
6. the common delivery floor passes.

## Handoff

Report the feature document, decision/delivery/readiness states, accepted and reserved forms, dependency/prerequisite closure, unsupported lowering paths, exact gates run, and remaining lifecycle or runtime work.
