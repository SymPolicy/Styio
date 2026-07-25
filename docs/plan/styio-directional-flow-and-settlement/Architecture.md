# Directional Flow and Operation Settlement Architecture

**Purpose:** Define the single directional-operation, static completion-summary, settlement-wrapper, binding, and one-shot deletion architecture required by `D21` and `Q01-A`.

**Last updated:** 2026-07-19

## 1. Architectural invariant

```text
left expression/action -> typed endpoint     = one directional operation
?| directional-or-other-operation | arms     = settlement around the operation
answer = settlement-expression              = ordinary binding around the result
```

The layers are nested and orthogonal. Endpoint kind never changes the graphical meaning of `->`; settlement never creates a declaration target; binding never changes operation semantics.

The canonical semantic data is:

```text
FamilyId           = interned nominal identity with stable semantic ordinal
CompletionSet      = canonical small bit-set keyed by FamilyId ordinal
OperationSummary   = { success_type, completion_set }
CompletionArmFact  = { family_id, optional_payload_symbol, body_summary }
SettlementSummary  = { success_type, remaining_completion_set }
```

Payload shape is stored once in the family symbol rather than copied into every set member. Unit remains a real logical success fact while requiring no physical result payload.

## 2. Module and responsibility map

| Layer/module | Single responsibility | Must not own |
|--------------|-----------------------|--------------|
| Parser directional production | Parse a left source and right endpoint into one directional node. | Task/resource/export-specific arrow productions or type-directed reinterpretation. |
| Endpoint AST/interface | Represent the typed destination/location/receiver and its source-compatibility contract. | A second meaning for `->` or generic settlement policy. |
| Settlement AST/interface | Wrap one operation with exact nominal completion arms and an optional recoverable fallback. | Target declarations, wildcard discard, arrow endpoint dispatch, or ordinary binding mutability. |
| Ordinary binding AST | Bind the produced settlement value through the mandatory RHS invariant. | Await/task lifecycle or effect handling. |
| Sema directional checker | Validate source-to-endpoint compatibility through endpoint contracts. | Glyph-specific branches named after backend subsystems. |
| Sema settlement checker | Consume the `Q01-A` operation summary; resolve exact families/payload binders, subtract handled families, join normal results, and propagate remaining families. | Creating a local target, dynamic matching, or implicit retry. |
| IR/lowering | Preserve directional operation, endpoint contract, settlement wrapper, and binding as separate typed facts. | `isAwaitBind`/`declaresTarget` compatibility flags. |
| Native/backend adapters | Execute direct endpoint and settlement branches already proven by typed IR. | Ambient error state, dynamic handler search, reconstructing source syntax, or deciding open language policy. |

## 3. Frontend structure

The parser exposes one directional-flow node conceptually equivalent to:

```text
DirectionalFlowAST {
  source: ExpressionOrAction
  endpoint: TypedEndpoint
}
```

`TypedEndpoint` is an interface boundary, not a dynamic registry. Existing endpoint AST families can implement or lower into that interface while their own grammar and protocol remain under their feature owners. This deliberately uses one Strategy-style typed endpoint contract because it removes parser/Sema role flags and permits family-specific validation without creating several arrow meanings.

Settlement is a separate node conceptually equivalent to:

```text
SettlementAST {
  operation: Operation
  arms: CompletionArm*
  fallback: Expression?
}

CompletionArm {
  family_name: Identifier
  payload_binding: Identifier?
  recovery: Expression
}
```

The operation may be directional or another operation accepted by the settlement contract. Family and payload names remain ordinary identifiers. The AST preserves syntax and spans only; semantic identity and payload type belong to Sema. A simple tagged node plus typed interfaces is sufficient; no visitor plug-in registry or managed runtime is needed. Statement/value use is a property of the surrounding consumer, not a discard mode in settlement.

## 4. Ordinary result binding

The binding parser sees the same mandatory RHS used for any other expression:

```styio
answer : T = ?| operation | fallback
```

It constructs an ordinary binding whose RHS is a settlement expression. Binding visibility, mutability/finality, type constraints, and failure-before-publication follow their existing owners. There is no await-target declaration, `declare_target_` flag, target-specific scope rule, or settlement-created local.

## 5. Generic composition

For an operation that includes a direction:

```styio
?| (operation -> destination) | fallback
```

Sema first checks the directional operation through the endpoint contract, producing success type `unit` plus its finite nominal completion-family set, then checks settlement over that operation summary. The parentheses are part of validation examples so this plan does not silently decide precedence or chaining. Lowering preserves the same nesting instead of flattening it into a task-specific bind.

## 6. Sema and IR contracts

Directional Sema receives a typed source and independently valid endpoint. It returns the accepted `Q01-A` operation summary: success type `unit` plus the complete source/endpoint operation's finite nominal completion-family set. Settlement Sema consumes that summary and applies the canonical exact-match, safe-fallback, join, and propagation algebra. Ordinary binding consumes the final normal value type.

Settlement checking is one memoized pass:

1. infer the operation once to `OperationSummary(S, C)`;
2. resolve each family and optional payload binder, rejecting unknown/duplicate families and invalid payload binding;
3. check each recovery in a fresh branch-local scope and require every normal result to join with `S`, while `never` contributes no normal value;
4. subtract exactly named IDs from `C`, then let a final catch-all subtract only remaining recoverable-failure families;
5. union every completion family produced by a reachable recovery expression;
6. canonicalize and memoize `SettlementSummary(S, remaining)`.

IR should preserve these facts with focused node families or interfaces:

```text
DirectionalOperation(source, endpoint, OperationSummary(unit, completion_set))
Settlement(operation, resolved_arms, fallback, result_type, remaining_completions)
Bind(destination_name, value)
```

The exact runtime scheduling, ownership transfer, buffering, and acknowledgement data structures cannot be designed until their owner questions are answered. This plan therefore preserves typed extension points without filling them with defaults.

IR carries resolved family IDs, typed payload projections, the normal success type, and the exact remaining set. Its verifier rejects duplicate IDs, payload-shape mismatches, invalid joins, and set inconsistencies. Backend lowering emits Q01-owned local conditional/switch edges over compiler-known ordinals while consuming Q03-F's bounded Outcome transport and payload layout. Q01 creates no unwinder, heap exception, dynamic family table, open-ended failure list, hidden `Result`, implicit retry, or default-value repair; Q03-F alone owns the ABI and complete ambient-state deletion.

## 7. One-shot deletion and migration

The final implementation deletes every route whose only purpose is the false specialized model, including:

- `looks_like_await_bind_stmt_nightly` and `parse_await_bind_stmt_nightly`;
- `FlowBindAST::CreateAwait`, `await_bind_`, and `declare_target_` when not required by generic flow;
- source-less `?| ->` Sema/lowering branches and diagnostics;
- task-only await-source parsing/type branches attached to the false binder;
- IR/codegen flags that distinguish an arrow as an await bind;
- settlement ellipsis parsing, `discard_` / `isDiscard()`, IR discard flags, and discard codegen blocks;
- hard-coded string-only completion matching when nominal symbols replace it;
- implicit fallback coercion, Q01-local result repair, and handler lookup paths; ambient runtime-error implementations and their transitive consumers are inventoried here only for handoff to the Q03-F deletion owner;
- positive fixtures and documentation that teach settlement-created targets.

Valid generic directional operations are migrated to the single directional node. Valid task/resource/effect settlement is migrated to the single settlement wrapper. Result capture is migrated to an ordinary binding. No compatibility AST, parser branch, runtime fallback, or positive legacy fixture remains.

## 8. Dependency direction

```text
Language decision
  -> directional grammar + endpoint interface
  -> directional Sema fact
  -> settlement wrapper + settlement Sema fact
  -> ordinary binding consumer
  -> typed IR/lowering/backend
  -> tests/tooling/docs
```

Endpoint families depend on the shared directional contract; the shared contract never imports a task/resource-specific meaning. Settlement depends on the operation interface; operations do not depend on settlement syntax.

## 9. Complexity and data structures

Parsing and type checking remain linear in source/AST size. Endpoint dispatch is an AST-kind or typed-interface dispatch, not repeated lookahead or source scanning. Removing target-declaration, await, and discard flags reduces state combinations. Completion sets use an inline canonical bit set keyed by interned family ordinal: membership is constant time and union/subtraction/equality are `O(words)`. A separately ordinal-sorted vector is built only for deterministic diagnostics, so hash/container iteration order never becomes semantics. Operation/callable summaries are memoized at the existing inference boundary. No second effect representation, dynamic registry, heap indirection, or concurrency primitive is introduced.

## 10. External architecture inputs and unresolved seams

Accepted `Q03-F` already fixes the source/endpoint preparation relation: source value and endpoint capability are independent strict prerequisites, and the arrow's left-to-right data direction is not a source-before-endpoint time edge. Two unordered order-sensitive preparations must be sequenced outside the transfer. That contract is owned by [Styio Functional Evaluation and Effect Ordering](../../design/Styio-Functional-Evaluation-and-Effect-Ordering.md) and its dedicated implementation plan; this Q01 migration consumes it without implementing its `EvaluationFacts`, evaluation DAG/CFG, diagnostics, or optimizer rights.

Later owner decisions must still define `<-`, arrow chaining and intermediate results, endpoint declaration/construction, move/borrow/copy/stream policy, buffering/backpressure escalation, explicit retry-feature admission, cancellation protocol, and concurrency scheduling. The accepted `Q02-BC` / `Q02-SIG` / `Q02-INF` callable-contract and principal-inference surface is implemented by its separate [single-owner plan](../Styio-Callable-Principal-Inference-Plan.md) rather than this Q01 migration. Those external contracts may enrich endpoint or operation contracts, but they may not split the surface meaning of `->`, reintroduce implicit retry, or weaken the accepted completion algebra.
