# Styio Block Completion and Bottom Type — Validation Matrix

**Purpose:** Map every Block completion and bottom-type requirement to executable, static, tooling, and final acceptance evidence.

**Plan:** `styio-block-completion-and-bottom-type`

**Last updated:** 2026-07-16

## Standing gates

| Gate | Command |
|---|---|
| Build | `cmake -S . -B build/default -DCMAKE_BUILD_TYPE=Release && cmake --build build/default -j` |
| Language features | `ctest --test-dir build/default -L language_feature --output-on-failure` |
| Pipeline | `ctest --test-dir build/default -L styio_pipeline --output-on-failure` |
| Security | `ctest --test-dir build/default -L security --output-on-failure` |
| Parser shadow | `benchmark/parser-shadow-suite-gate.sh` |
| Syntax convergence | `python scripts/syntax-convergence-gate.py` |
| Documentation | `python scripts/docs-index.py --check && python scripts/docs-audit.py && python scripts/docs-lifecycle.py validate` |
| Plan state | `python scripts/manifest_tool.py validate docs/plan` |
| Local-information safety | `python scripts/local-info-leak-gate.py --mode worktree` |

## Requirement mapping

### REQ-BC-001 — Reachable fallthrough is `unit`

- Parser/Sema unit tests cover empty Blocks, statement-only Blocks, and direct `()` expressions.
- Function feature fixtures prove a statement-only/empty body has logical `unit`, with no observable integer payload.
- IR goldens show no `i64 0` synthesized solely because a Block produced no business value.

### REQ-BC-002 — Normal-path joins

- Positive fixtures: every reachable exit produces the same scalar/string/unit type.
- Negative fixtures: one branch yields `i64` while another reaches `}`; one branch yields `i64` while another yields `()`; diagnostics identify both origins.
- Static grep/review proves Block/function-result lowering has no path that repairs a failed join with a default or optional lift.

### REQ-BC-003 — Public `never`

- Type parser tests accept `never` in every ordinary type position supported by the grammar.
- Negative tests reject a `never` literal, construction, binding initializer, default value, and reachable function fallthrough declared as `never`.
- Positive tests prove an infinite/proven-diverging branch joins with `T` as `T`.
- IR verification proves a `never` path produces no value; codegen emits `unreachable` only after Sema proves divergence.

### REQ-BC-004 — Lexical Block and function composition

- Equivalence fixtures compare `=> expr`, `=> { expr }`, and `=> { <| expr }` for identical types and stdout.
- Nested fixtures prove inner `<|` completes only the inner Block and outer statements continue.
- A multi-item value Block without `<|` fails when a non-`unit` result is required.
- Existing tests that expected nested `<|` to exit the function are migrated or removed in the same change; none remain as acceptance coverage.

### REQ-BC-005 — No keyword/token

- Tokenizer tests assert source `never` produces `StyioTokenType::NAME`.
- Static inspection proves no `NEVER`/`KW_NEVER` token or keyword dispatch was added.
- Value-namespace use follows the language's namespace decision; this plan only reserves built-in meaning in type position.

### REQ-BC-006 — Diagnostics and convergence

- Stable diagnostic codes cover incompatible Block results, missing non-unit result, invalid `never` value/default, and same-Block unreachable statements.
- AST/visitor inventories contain `BlockYieldAST` and no `ReturnAST` compatibility type.
- IR inventories contain distinct Block-result and function-return nodes, with verifier tests for malformed joins.
- Active language docs, EBNF, symbol reference, syntax matrix, editor grammar, formatter, and runbooks agree.

### REQ-BC-007 — Strict yield surface and consumers

- Parser/AST tests prove `<| expr` and `|<| expr |;` create the same
  `BlockYieldAST`, owner identity, and source result; negative parser tests prove
  omitted or malformed inline `|;` is rejected.
- Positive parser/Sema tests prove `<| ()` is legal in value and Unit-only
  contexts and completes the remaining current Block.
- Negative Sema tests prove a Unit-only iterator/body consumer rejects `<| T`;
  no lowering or codegen path evaluates and discards the value.
- Structural-flow tests reject a direct sibling after unconditional completion
  and a region whose every incoming edge has completed, while accepting a
  sibling when at least one incoming edge can continue. Diagnostics remain
  unchanged across optimization levels.
- IR verifier and inventory checks prove both source spellings lower through one
  lexical Block-result form and never create an inline-specific terminator or
  function-return path.

## Final end-to-end acceptance

On one head commit:

1. Run the build and all standing gates.
2. Run all `REQ-BC-*` targeted positive/negative tests and IR verifier tests.
3. Search the active tree for obsolete `ReturnAST`/cross-Block-return wording and classify every remaining match; no compatibility implementation or active acceptance fixture remains.
4. Confirm tokenizer output for `never` is still `NAME` and the public token inventory did not grow.
5. Confirm both yield spellings have identical AST/owner/IR structure, `<| ()`
   succeeds, missing inline `|;`, structural unreachability, and Unit-only
   non-Unit yields fail with stable diagnostics.
6. Record each requirement's evidence in the final-validation checkpoint, then run `scripts/manifest_tool.py sync-plan docs/plan` and `validate docs/plan`.
