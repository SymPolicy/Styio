# Directional Flow and Operation Settlement Validation

**Purpose:** Map every `REQ-DFS-*` and `REQ-OCS-*` contract to positive, negative, structural, runtime-free, migration, and documentation evidence.

**Last updated:** 2026-07-19

## Validation principles

1. Every endpoint-specific positive case must prove the same directional AST/Sema interface, not merely the same token.
2. Settlement tests must distinguish the operation from the wrapper and ordinary result binding from both, while composing with the accepted `Q01-A` completion algebra rather than preserving legacy behavior.
3. Specialized await-target and bare-freeze routes are deleted structurally; parser rejection alone is insufficient.
4. Tests must not encode answers for the explicitly open ownership, scheduling, chaining, reverse-flow, and backpressure questions.

## Requirement matrix

| Requirement | Positive evidence | Negative/static evidence |
|-------------|-------------------|--------------------------|
| `REQ-DFS-001` | Representative value/action sources flow into typed destination/location/receiver endpoints through one directional node. | No endpoint family maps `->` to an unrelated operator kind or grammar production. |
| `REQ-DFS-002` | Endpoint-specific type/capability checks dispatch from the common endpoint interface. | AST/Sema/IR searches find no task-arrow/resource-arrow/export-arrow semantic flags or duplicate operator precedence. |
| `REQ-DFS-003` | `?| operation`, recoverable fallback, and exact `family` / `family(binding)` forms wrap one operation and preserve its `Q01-A` summary. | Settlement does not declare a target, accept wildcard discard, or reparse an inner arrow according to task/resource category. |
| `REQ-DFS-004` | `answer : T = ?| operation | fallback` and mutable/final variants use ordinary binding tests. | No settlement-only declaration node, target scope rule, or hidden uninitialized target remains. |
| `REQ-DFS-005` | `?| (operation -> destination) | fallback` settles the whole generic transfer where that operation is supported. | The wrapped arrow is not relabeled as a task binder, resource-only redirect, or export-only operator. |
| `REQ-DFS-006` | Current generic flow and settlement fixtures remain positive after migration. | `parse_await_bind_stmt_nightly`, await lookahead, `CreateAwait`, `isAwaitBind`, `declaresTarget`, bare-freeze branches, and positive specialized fixtures are absent. |
| `REQ-DFS-007` | Plan/report non-goals and routed findings explicitly retain every adjacent owner question. | No new test asserts unapproved `<-`, chaining, ownership, evaluation, buffering, backpressure, scheduling, or cancellation policy. |
| `REQ-DFS-008` | Compiler layers, active SSOT, formatter/editor mirrors, runbooks, and generated indexes agree. | Convergence/static gates find no task-specific await-target explanation or compatibility execution path. |
| `REQ-OCS-001` | Pure, Unit, value, and multi-family operations expose one canonical summary. | No ordinary `Result` or undefined/default success type; Q01 paths consume the Q03-F Outcome interface and never consult ambient failure state. |
| `REQ-OCS-002` | Arbitrary family/binder names tokenize as identifiers; renaming a binder preserves behavior. | Keyword/token tables contain no `io`, `err`, or family-specific entry. |
| `REQ-OCS-003` | Existing variable/resource/task/terminal endpoints produce Unit on successful direction. | Undeclared/non-capable targets fail; no source/destination/receipt result route. |
| `REQ-OCS-004` | Side-effect counters prove one operation and one selected recovery. | Success and unselected arms stay untouched; no retry/replay loop. |
| `REQ-OCS-005` | Bare exact arm and payload-binding arm resolve nominally with local scope. | Unknown, duplicate, no-payload binding, scope leak, and inheritance-like matching fail. |
| `REQ-OCS-006` | A final catch-all recovers remaining recoverable failures. | EOF/cancel/shutdown/fatal are not swallowed; no named arm follows fallback. |
| `REQ-OCS-007` | Scalar/container/Unit joins and `join(S, never)` preserve `S`. | Mixed normal results fail before IR; no default/Optional/Unit-discard repair. |
| `REQ-OCS-008` | Nested recovery completions and partial handling expose exact remaining sets. | No unhandled family disappears and no handled family remains accidentally. |
| `REQ-OCS-009` | Absence, EOF, failure, cancel, shutdown, fatal, and pressure cases remain distinct. | Optional flattening or catch-all cannot collapse categories. |
| `REQ-OCS-010` | Explicit exact family-to-Unit recovery works in a Unit consumer. | `?| op | ...` fails everywhere; discard symbols/flags/tests are absent. |
| `REQ-OCS-011` | IR/verifier/backend use finite IDs, Q01 local direct branches, and the Q03-F bounded Outcome layout. | No heap exception, dynamic table/search, unwinder, open list, truncation, or Q01 ambient-state consumer; complete global/TLS/JIT/CLI deletion is evidenced by the Q03-F receipt. |
| `REQ-OCS-012` | Compiler, tooling, docs, plans, indexes, and gates agree on one head. | No positive compatibility fixture or contradictory active prose. |

## Required source cases

### Accepted contract shapes

```styio
answer : T = ?| operation | fallback
final_answer : T := ?| operation | io(problem) => recover(problem)
?| (operation -> destination) | fallback

value = ?| operation
        | eof => eof_value
        | network(problem) => recover(problem)
```

`io`, `problem`, `network`, and `eof` in these examples are ordinary identifiers resolved by Sema; none is a keyword. Concrete operations and endpoints come from their owning feature fixtures. These examples freeze composition and completion behavior only; they do not freeze arrow precedence, chaining, ownership, public completion signatures, family declarations, or scheduling beyond the explicit parentheses.

### Removed specialized shapes

The implementation must not own dedicated productions equivalent to:

```styio
?| task -> name: T
?| -> name: T
?| operation | ...
?| operation | unknown(x) => fallback
?| operation | no_payload(x) => fallback
?| operation | io(a) => x | io(b) => y
?| operation | fallback | io(e) => recover(e)
source -> newly_declared: T
```

Rejected source coverage may retain these strings only as current diagnostics. A syntactically similar arrow inside a generic parenthesized operation is not the removed task binder.

## Structural inspections

1. Search tokenizer/parser/AST/Sema/IR/lowering/codegen/tests for await-target lookahead, `CreateAwait`, `isAwaitBind`, `declaresTarget`, and bare-freeze messages; no executable match remains.
2. Inspect every retained `FlowBindAST` or replacement directional node and prove it has one endpoint contract rather than subsystem-role flags.
3. Inspect settlement AST/IR and prove its operands are operation plus exact completion arms/fallback, not a declaration target or discard mode; verify exact IDs, payload types, join type, and remaining set.
4. Inspect ordinary binding tests and prove settlement-result bindings use the mandatory-RHS lifecycle.
5. Classify every remaining task/resource/flow arrow test as an endpoint-specific check below the common direction rule.
6. Search active documents and runbooks for task-specific await-target, arrow-alias, or source-less freeze statements.
7. Search parser/AST/Sema/IR/lowering/codegen/repr/tests for `isDiscard`, `discard_`, ellipsis settlement fixtures, hard-coded family lists, and default/result repair; no executable or positive compatibility Q01 route remains. Separately prove Q01 paths consume the Q03-F Outcome interface, and link the Q03-F receipt for complete ambient runtime-error removal instead of treating it as a Q01 deletion.
8. Search tokenizer and editor syntax tables for family-specific keywords; none exists.
9. Inspect memoized operation summaries and canonical completion-set algebra; operation inference occurs once and deterministic diagnostics do not depend on hash order.

## Exact-once and category stress cases

1. Success, every named arm, and catch-all own observable counters; exactly one valid path changes state.
2. A recovery operation that completes abnormally contributes its family to the outer summary.
3. Catch-all receives remaining recoverable I/O/parse/bounds families but not EOF, cancellation, or shutdown.
4. A pressure pulse alone never enters settlement; only an explicitly escalated resource completion family can be named.
5. Directional source and endpoint failures both prevent Unit success; no later endpoint status overwrites an earlier completion.
6. Block candidate publication waits until settlement and required exit obligations complete.

## Targeted commands

Exact compiler targets are finalized by the evidence/test-discovery checkpoint. Final validation includes affected parser, AST, Sema, IR, lowering, codegen, task/resource/effect, security, and pipeline tests, followed by:

```text
python scripts/syntax-convergence-gate.py
python scripts/docs-index.py --check
python scripts/docs-audit.py
python scripts/docs-lifecycle.py validate
python scripts/local-info-leak-gate.py --mode worktree
python scripts/manifest_tool.py validate docs/plan
```

## Final evidence record

The final-validation node records the head commit, command outcomes, per-requirement evidence, removed symbol/test inventory, retained endpoint and completion families, measured completion-set representation, active documentation set, and every adjacent finding routed to the accepted `Q02` or `Q03-F` implementation owner or to the active `Q04`/`Q09` owner. It must confirm that `->` remains a left-to-right data direction without inventing source-before-endpoint preparation order. Q01 acceptance cannot be weakened to preserve a task-specific binder, wildcard discard, Q01-local ambient-state consumer, duplicate Q03-F machinery, or unapproved flow policy; complete global/TLS/JIT/CLI deletion is linked as a Q03-F-owned prerequisite/receipt.
