# Directional Flow and Operation Settlement Requirements

**Purpose:** Define the frozen graphical data-flow meaning of `->`, the complete `Q01-A` operation-completion algebra, and their orthogonal implementation as one migration.

**Last updated:** 2026-07-19

## Decision trace

`D21` in the language decision ledger records the owner clarification that `a -> b` is not a collection of task-binding, resource-writing, export, redirect, or assignment meanings. It is one graphical direction rule: data produced by the expression or action on the left flows to the location represented by the endpoint on the right. The owner also froze `?| operation | fallback` as generic operation settlement and accepted ordinary result binding as `answer = ?| operation | fallback`.

On 2026-07-19 the owner accepted `Q01-A` in full and clarified that `io(err)` adds no keyword: `io` is an example nominal completion-family identifier and `err` is an author-chosen branch-local payload binding. The normative completion owner is [Styio Operation Completion and Settlement](../../design/Styio-Operation-Completion-and-Settlement.md). This plan is the single implementation owner for `D21` and `Q01-A`; direction cleanup, generic settlement, wildcard-discard deletion, and task-target deletion land together.

## Users and outcomes

1. Authors can read the source as a data-flow graph without memorizing subsystem-specific arrow meanings.
2. Library and endpoint authors can define typed destinations or receivers without minting a new surface operator role.
3. Task, resource, and effect implementations share one settlement wrapper instead of introducing target-declaration syntax.
4. Parser, compiler, formatter, and documentation maintain one compositional grammar and one explanation.

## Functional requirements

### REQ-DFS-001 — One left-to-right flow meaning

For `left -> destination`, the value/data produced by the left expression or action flows to the right destination, location, or receiver endpoint. This graphical direction is the only source-level meaning of `->`.

### REQ-DFS-002 — Endpoint categories do not create operator polysemy

Endpoint-specific type, capability, ownership, protocol, and lowering rules are selected by the endpoint contract. Labels such as assignment, export, redirect, resource write, or task result delivery are implementation/context classifications, never alternative surface meanings of `->`.

### REQ-DFS-003 — Settlement is an orthogonal operation wrapper

`?| operation | fallback` and exact `family` / `family(binding)` arms settle the complete operation. `?|` neither declares a target nor changes any `->` contained by the operation. Accepted `Q01-A` fixes success values, static completion facts, matching, payloads, joins, propagation, and wildcard-discard rejection; this plan owns their compiler representation and validation.

### REQ-DFS-004 — Settlement results use ordinary bindings

A source binding receives a settlement result through the ordinary mandatory-RHS form, for example `answer : T = ?| operation | fallback`. Mutability/finality and explicit type constraints remain properties of `=` / `:=` and `:`, not of `?|`.

### REQ-DFS-005 — Generic directional operations compose with settlement

When a directional transfer is a settleable operation, the settlement wrapper may enclose it, for example `?| (operation -> destination) | fallback`. The arrow retains REQ-DFS-001; settlement observes the outcome of the entire transfer operation.

### REQ-DFS-006 — No task-specific await-target or bare-freeze production

The compiler exposes no dedicated `?| task -> name: T` target-declaration production and no source-less `?| -> name: T` production. Parser lookahead, AST/Sema/IR flags, lowering branches, diagnostics, and positive fixtures that exist solely for those interpretations are removed rather than retained as compatibility.

### REQ-DFS-007 — Adjacent owner boundaries remain external

This delivery does not decide reverse-flow spelling, chaining, intermediate results, endpoint declaration grammar, ownership/borrowing/copy policy, buffering, backpressure escalation, scheduling, cancellation protocol, or completion-family declaration ownership. It also does not implement the separately accepted `Q02-BC` / `Q02-SIG` / `Q02-INF` callable-contract and principal-inference surface, which has its own [Callable Principal Inference plan](../Styio-Callable-Principal-Inference-Plan.md), or the accepted `Q03-F` strict-value/dependency-graph/effect-order contract, which is owned by [Styio Functional Evaluation and Effect Ordering](../../design/Styio-Functional-Evaluation-and-Effect-Ordering.md) and its dedicated implementation plan. This migration must conform to those accepted contracts without absorbing their implementation scope or pre-empting another active owner question. Exact local settlement matching, propagation, once-only evaluation, safe fallback, and no implicit retry are already fixed by `Q01-A` and are not open here.

### REQ-DFS-008 — One converged source and compiler contract

Language SSOT, parser, AST, Sema, IR, lowering, codegen, diagnostics, tests, formatter/editor mirrors, and runbooks use the same directional-flow and settlement composition. Removed specialized paths have no executable compatibility route.

### REQ-OCS-001 — One typed operation summary

Every operation has exactly one canonical success type and one canonical finite set of nominal completion families. Pure operations have an empty set. The summary is a static type fact, not an ordinary `Result` value or ambient failure channel.

### REQ-OCS-002 — No new keyword

Completion-family names and payload bindings tokenize as ordinary identifiers. `family(binding)` adds a settlement-arm grammar shape only; no example family or binder spelling is reserved.

### REQ-OCS-003 — Directional Unit success and existing destination

Successful `source -> destination` produces `() : unit`. The arrow never returns the source, destination, or receipt and never declares a name. Its destination must independently resolve to a legal endpoint with the required capability.

### REQ-OCS-004 — Local exact-once evaluation

Settlement evaluates its operation once. Success evaluates no recovery arm. Only the selected recovery arm is evaluated, lazily and once. Settlement never replays or retries the operation implicitly.

### REQ-OCS-005 — Exact nominal named arms

`family => recovery` exactly matches one family without a payload binding. `family(binding) => recovery` binds its typed payload in that arm only. Binding a no-payload family, naming an unknown family, or repeating a family is an error. No inheritance or dynamic handler search participates.

### REQ-OCS-006 — Catch-all boundary

A bare final fallback matches only remaining recoverable failure families. It must be last and never matches absence, EOF, cancellation, shutdown, or fatal/trap.

### REQ-OCS-007 — Result join

The operation success and every normally completing recovery arm have one compatible canonical success type. `never` contributes no normal value. The compiler never inserts Unit discard, default, zero, Optional, or an invented union to repair a conflict.

### REQ-OCS-008 — Static propagation

The enclosing summary contains every unhandled operation family plus every completion family produced by recovery expressions. An exact arm removes only its family; catch-all removes only remaining recoverable failures.

### REQ-OCS-009 — Completion categories remain distinct

Absence is `? | T`; EOF is a terminal family; recoverable failures are nominal families; cancellation and shutdown are control-terminal families; fatal/trap is outside settlement; pressure is a signal until a resource protocol escalates it. No representation or branch collapses these categories.

### REQ-OCS-010 — No wildcard discard

`?| operation | ...` is rejected in all contexts. Parser, AST, Sema, IR, lowering, backend, repr, diagnostics, tests, and docs contain no executable discard route or positive compatibility fixture.

### REQ-OCS-011 — Runtime-free bounded representation

Completion sets, family identities, and payload shapes are compiler-known and bounded. Q01 settlement uses direct typed control flow and the Q03-F compiler-bounded Outcome transport, not heap exception objects, open-ended lists, stack unwinding, ambient error guards, or dynamic handler lookup. Q01 owns the local settlement edges; Q03-F alone owns the cross-producer ABI and complete global/TLS/JIT/CLI ambient-state deletion.

### REQ-OCS-012 — One-shot convergence

All compiler layers, editor/formatter mirrors, examples, runbooks, plans, and gates migrate together. Removed routes are deleted rather than warned or kept as long-lived compatibility.

## Non-functional constraints

1. Parsing remains syntax-directed; endpoint classification follows typed AST contracts rather than rescanning source text in each subsystem.
2. The common directional node and settlement wrapper do not erase endpoint-specific type or capability checks.
3. The migration reduces special cases and does not add a managed runtime, dynamic endpoint registry, or general implicit conversion mechanism.
4. Diagnostics name the failing endpoint or settlement contract without describing `->` as several unrelated operators.
5. Every deletion is one-shot; retired parser/AST/Sema/IR routes and their positive tests disappear together.

## Scope

Directional-flow and settlement parsing in `src/StyioParser`; `FlowBindAST` and settlement ownership in `src/StyioAST`; nominal completion symbols, summaries, joins, propagation, endpoint validation, Sema/IR/lowering/codegen paths; affected task/resource/operation fixtures; editor/formatter mirrors; active language documents; runbooks; and gates.

## Non-goals and follow-up owner questions

- What `<-` means and whether it is related to `->`.
- Whether and how directional flows chain through intermediate endpoints.
- The exact runtime evaluation/scheduling order of source and endpoint work.
- Move, borrow, copy, streaming, buffering, explicit retry-feature admission, backpressure escalation, and cancellation protocol.
- The full typed-endpoint declaration/construction grammar.
- New completion-family declaration, public-signature, task-lifecycle, or resource-family semantics beyond `Q01-A`.
- Chained arrows, fan-out, continuation, an implicit/explicit retry operator, effect inheritance, or a user-defined completion-family declaration surface.

## Final acceptance target

On one head commit, the compiler has one directional-flow representation, one canonical typed operation summary, and one generic settlement wrapper; ordinary bindings receive settlement values through a real RHS; exact named/payload arms and safe fallback work lazily once; generic transfer operations succeed with Unit and remain composable; wildcard discard and specialized await-target/bare-freeze paths are absent; every `REQ-DFS-*` and `REQ-OCS-*` requirement has recorded evidence; and no open adjacent question has been silently implemented as policy.
