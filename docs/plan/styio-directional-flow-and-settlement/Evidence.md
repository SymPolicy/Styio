# Directional Flow and Operation Settlement Evidence

**Purpose:** Ground the unified arrow and accepted `Q01-A` settlement contract in repository contradictions and established language-design experience.

**Last updated:** 2026-07-19

## Repository evidence

| Evidence | Current fact | Requirement impact |
|----------|--------------|--------------------|
| `docs/design/Styio-Symbol-Reference.md` arrow rows | The design already depicts `->` as forward movement into resource/destination endpoints, while other documents reclassify a similar shape as task-specific binding. | Supports one graphical rule in `REQ-DFS-001..002`; subsystem labels must not become operator meanings. |
| Git-baseline `docs/design/Styio-EBNF.md` `await_stmt` before the current documentation correction | The baseline grammar added `?| [expression] -> identifier : type`, including a source-less form, beside the generic resource-effect settlement production; the corrected worktree design has already removed it. | Preserve the corrected authority and delete the still-current compiler specialization under `REQ-DFS-003..006`. |
| `src/StyioParser/NewParserExpr.cpp:2144-2185` | `parse_await_bind_stmt_nightly` requires a typed target and constructs `FlowBindAST::CreateAwait`. | The parser currently turns settlement into a target declaration instead of producing a settlement expression for ordinary binding. |
| `src/StyioParser/NewParserExpr.cpp:2188-2241,2430-2434` | Lookahead splits `?|` into a special await-bind parser or the generic resource-effect parser. | Replace the semantic fork with one settlement wrapper; operation typing belongs below syntax classification. |
| `src/StyioAST/AST.hpp:5053-5115` | `FlowBindAST` contains `declare_target_` and `await_bind_` flags plus `CreateAwait`, mixing generic flow with task-specific declaration state. | Refactor to one directional operation and remove specialized flags (`REQ-DFS-002`, `REQ-DFS-006`). |
| `src/StyioSema/TypeInfer.cpp:4210-4285` | Sema has source-less continuation errors, task-only await checks, declaration/reassignment branches, task consumption, and fallback target typing in one flow node. | Separate generic flow endpoint checking from generic settlement typing; do not preserve the false surface model downstream. |
| `src/StyioLowering/AstToStyioIR.cpp:4656-4680` | Lowering carries the source-less branch and `isAwaitBind()` into IR. | The specialized interpretation has crossed multiple layers and requires one complete deletion, not a documentation-only correction. |
| `tests/newparser_internal_test.cpp`, `tests/typeinfer_internal_test.cpp`, `tests/security/styio_security_test.cpp` | Positive tests construct `CreateAwait`, task-target declarations, and bare-freeze diagnostics. | Tests currently protect an unauthorized design and must converge with source, compiler, and docs in the same migration. |
| Existing `ResourceEffectAST` / `SIOResourceEffect` tests | The repository already has a wrapper-shaped settlement path with operation, fallback, string handlers, value/discard state, and typed results. | Reuse the generic wrapper responsibility while broadening operation ownership; replace strings with nominal family IDs and optional binders, delete discard, and do not rebuild task settlement as another binder. |

### Completion-algebra implementation paths

| Evidence | Current fact | Required action |
|---|---|---|
| `src/StyioParser/NewParserExpr.cpp:2323-2413` | Named handlers, fallback, and `...` discard are parsed in one resource-effect route. | Parse optional ordinary-identifier payload binders and delete ellipsis parsing. |
| `src/StyioAST/AST.hpp:3067-3152` | `ResourceEffectAST` owns string handlers/fallback plus a `discard_` flag. | Keep syntax-only family/binder nodes and delete discard state. |
| `src/StyioSema/TypeInfer.cpp:2908-2970` | Current Sema knows a hard-coded family list, discard, and use-site fallback typing. | Resolve nominal families, build canonical completion sets, and perform exact subtraction/union plus canonical normal-result joins. |
| `src/StyioIR/GenIR/SIOIR.hpp:284-332` | `SIOResourceEffect` preserves handler strings and a discard boolean. | Carry resolved family ID, optional typed payload symbol, result type, and remaining completion set; delete discard. |
| `src/StyioLowering/AstToStyioIR.cpp:3543-3556` | Lowering copies current AST flags directly. | Consume Sema-proven summaries only; never re-infer matching or repair joins. |
| `src/StyioCodeGen/CodeGenG.cpp:6089-6205` | Backend has dedicated discard and fallback blocks and may meet ambient error consumers. | Emit Q01-verified local branches, remove discard and implicit repair/coercion, consume the Q03-F Outcome ABI, and route every ambient-state implementation/consumer to Q03-F's sole deletion inventory. |
| Internal/security tests | Positive tests construct discard flags, hard-coded family strings, and task-await bind state. | Replace them with arbitrary identifier/binder, join, propagation, category, exact-once, Unit, and deletion evidence. |

## Other-language experience

### Await/settlement works best as an expression

- The [Rust Reference await expression](https://doc.rust-lang.org/reference/expressions/await-expr.html) defines `.await` as an expression whose result is the ready value. Ordinary `let`/assignment composition remains outside await syntax.
- The [C# language specification](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/expressions#1299-await-expressions) likewise classifies `await t` as a value when `GetResult` returns `T`; ordinary assignment remains ordinary assignment.

The transferable lesson is structural, not a request to copy either runtime: settlement should produce an expression result, so the language does not need a second declaration grammar tied to tasks.

### Direction syntax fails when implementation categories become public meanings

Pipeline and data-flow notations remain teachable when the glyph states one stable direction and endpoint contracts supply the detailed behavior. The common failure is to document each backend route as another operator meaning: precedence, diagnostics, formatter rules, generic abstraction, and refactoring then diverge even though users still see the same arrow. Styio avoids that split by making endpoint kind a typed contract below one graphical `left -> right` rule.

### Orthogonal composition prevents grammar growth

Languages that keep effectful/asynchronous completion expression-shaped can reuse ordinary binding, return, argument, and composition syntax. Adding a task-target declaration to the settlement marker duplicates those facilities and later forces special cases for mutability, type annotations, scope, patterns, and chaining. Styio keeps `?|` responsible only for settlement and `->` responsible only for direction.

### Failure models to avoid

- [Java checked exceptions](https://docs.oracle.com/javase/specs/jls/se21/html/jls-11.html) make failures part of API contracts, but class-hierarchy matching and dynamic unwinding couple recovery to inheritance and a runtime stack mechanism. Styio uses exact nominal families and direct static control flow.
- [Go error values](https://go.dev/blog/error-handling-and-go) are easy to forward but can be ignored like ordinary values. Styio completion is a non-discardable static operation fact.
- [Rust `Result` and `?`](https://doc.rust-lang.org/reference/expressions/operator-expr.html#the-question-mark-operator) make propagation explicit, but mandatory wrappers/conversions spread through ordinary value types. Styio keeps completion separate from `S` and from Optional.
- [Bash pipelines](https://www.gnu.org/software/bash/manual/bash.html#Pipelines) demonstrate how a final success can hide an earlier failure without `pipefail`. A directional operation produces Unit only after the complete source-to-endpoint action succeeds.
- [Reactive Streams](https://github.com/reactive-streams/reactive-streams-jvm) separates normal termination, failure, cancellation, and pressure. Styio preserves these categories instead of feeding all of them to fallback.
- [Kotlin cancellation](https://kotlinlang.org/docs/cancellation-and-timeouts.html) documents how broad catching can swallow cancellation. Styio's bare fallback excludes cancellation and shutdown.

## Inference versus evidence

Repository code proves that specialized await-target, bare-freeze, hard-coded family, and wildcard-discard routes exist and are cross-layer, while the owner decisions prove that those routes do not define the language. The proposed common AST/module boundary and completion-set representation are implementation consequences of that correction. They do not implement the accepted Q02-BC/Q02-SIG/Q02-INF callable-contract and principal-inference surface, whose implementation belongs to the [Callable Principal Inference plan](../Styio-Callable-Principal-Inference-Plan.md); nor do they decide endpoint ownership, scheduling, chaining, resource escalation policy, or reverse-flow behavior.

## Migration evidence to collect

1. Classify every `FlowBindAST`, `CreateAwait`, `isAwaitBind`, `declaresTarget`, await-lookahead, and bare-freeze use by whether it belongs to generic directional flow or the removed specialized route.
2. Classify every `?|` test as generic settlement, ordinary result binding, generic directional-operation composition, or obsolete task-target/bare-freeze behavior.
3. Prove all valid `->` endpoint families still pass through one directional semantic interface after refactoring.
4. Prove settlement result bindings use ordinary `BindingAST`/Sema paths and never create a hidden target declaration.
5. Record unresolved ownership, evaluation, chaining, backpressure, and `<-` findings under later owner questions rather than resolving them in code.
6. Inventory every settlement family string, discard flag, fallback block, Q01-local result repair, and positive compatibility fixture before Q01 deletion; classify every ambient runtime-error guard as a Q03-F handoff and require its completed deletion receipt rather than re-owning it here.
7. Prove arbitrary family/binder identifiers, exact matching, payload scope, safe catch-all, joins, propagation, category separation, and once-only recovery across all compiler layers.
