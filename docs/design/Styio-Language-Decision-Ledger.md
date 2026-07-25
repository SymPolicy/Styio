# Styio Language Decision Ledger

**Purpose:** Map every frozen or already accepted language decision to exactly
one normative design owner. This ledger is an index, not a second semantic
specification and not an implementation-status report.

**Last updated:** 2026-07-20

## 1. Authority model

| Concern | Authority |
|---|---|
| Language semantics | [Styio-Language-Design.md](./Styio-Language-Design.md) and the linked focused design |
| Formal source grammar | [Styio-EBNF.md](./Styio-EBNF.md) |
| Glyph/token lookup | [Styio-Symbol-Reference.md](./Styio-Symbol-Reference.md) |
| Compact accepted authoring surface | [syntax/ACTIVE-SYNTAX.md](./syntax/ACTIVE-SYNTAX.md) |
| Unresolved owner choices and evidence | [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md) |
| Parser/Sema/backend availability and migrations | Convergence matrix, rollups, and Better Plans |

An accepted design can be unimplemented. Conversely, a token, parser branch,
AST node, test fixture, or compatibility route is not a language decision. The
two states must never be collapsed into one `converged` label.

## 2. Frozen owner decisions

| Decision | Frozen contract | Unique semantic owner | Grammar / lookup mirrors |
|---|---|---|---|
| `O01-Q01` | Ordinary `T` never contains absence; every possibly missing value is statically `? | T`. | Language Design §3.5 | EBNF §4.2/§8.3; Symbol §7; Active §Types |
| `O01-Q02` | `unit` is first class and has exactly one value, `()`; it is distinct from absence, failure, EOF, uninitialized state, `never`, and `@()`. | Language Design §3.3.2 | EBNF `unit_value`; Symbol §7; Active §Types |
| `O01-Q03` | `=> expr`, `=> { expr }`, and `=> { <| expr }` are equivalent only for a sole-expression body; there is no general implicit tail result. | [Lexical Block Completion](./syntax/BLOCK_COMPLETION.md) §2 | EBNF §7; Symbol `=>`; Active §Block result model |
| `O01-Q04` | Reachable Block fallthrough produces Unit; incompatible normal exits fail closed and never synthesize a default or Optional. | [Lexical Block Completion](./syntax/BLOCK_COMPLETION.md) §3 | EBNF §7; Active §Block result model |
| `O01-Q05` | `never` is the contextual public bottom type, has no value/default, and joins only as proven non-completion. | Language Design §3.3.2 and §6.7 | EBNF §4.2/§7; Symbol §7 |
| `O01-Q06` | `? | T` is the only Optional type; `(?)`, `[?]`, and `{?}` are the same empty value; expected `T` injects the present branch. | Language Design §3.5 | EBNF `optional_type` / `optional_empty`; Symbol §7; Active §Types |
| `O01-Q07` | Repeated absence normalizes as `? | (? | T) == ? | T`; distinct empty meanings need an explicit tagged type. | Language Design §3.5 | EBNF §4.2; Symbol §7 |
| `O01-Q08..Q09` | Every ordinary binding has an explicit RHS; missing syntax never requests zero, Unit, absence, uninitialized storage, or a default. | Language Design §3.3.1 | EBNF §5; Symbol §6; Active §Binding Model |
| `O01-Q10..Q12` | Unit remains a normal generic argument; logical count/state is independent of physical payload; fallible no-payload success is Unit; explicit FFI adapters map returning `void`, nullable, and no-return facts to `unit`, `? | T`, and `never`. | Language Design §3.3.2 | Symbol §7; Active §Types |
| `O05-Q01` | `<| expr` completes only the current lexical Block; only the outer function-body Block result becomes the function result. | [Lexical Block Completion](./syntax/BLOCK_COMPLETION.md) §1 | EBNF `block_yield`; Symbol §2 |
| `O05-Q03..Q05`, `O05-Q07` | Inline `|<| expr |;` has the same node/target and mandatory terminator; `<| ()` is legal; structural unreachability is an error; Unit-only consumers reject non-Unit yields. | [Lexical Block Completion](./syntax/BLOCK_COMPLETION.md) §1/§3 | EBNF §7; Symbol §2; Active §Block result model |
| `O05-Q06`, `P01.12-A/B` | Candidate readiness precedes publication; required lexical obligations settle through one deterministic graph; bounded typed failures use fixed compiler-owned state without a managed runtime. | [Lexical Block Completion](./syntax/BLOCK_COMPLETION.md) §4 and Language Design §6.7.1 | No new grammar/token; implementation architecture is in the Block-exit Better Plan |
| `D02` / `O02` | Styio has no ordinary value fallback/coalescing operator; bare binary `|` and `??` are rejected before Sema. | Language Design §7.4/§13.4 | EBNF §8; Symbol §3/§9; Active §Pipe-role boundary |
| `D21` / `O29` | `left -> right` has one graphical meaning: data produced on the left flows to the endpoint on the right. `?|` is orthogonal and wraps the complete operation; settlement never declares a target. | Language Design §8.6 and §6.9 | EBNF §7.4/§8.2; Symbol §2; Active §Directional transfer axiom |
| `Q01-A` | Every operation has one success type plus finite nominal completion families as static facts. `?|` evaluates one operation once; exact `family` / `family(binding)` arms and a final recoverable-failure fallback are lazy and exact-once; normal results join to the success type; unhandled families propagate. Absence, EOF, failure, cancel, shutdown, fatal, and pressure remain distinct. Directional transfer succeeds with Unit and never declares its destination. Wildcard discard and implicit retry are rejected. Family/binding names are identifiers, not keywords. | [Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md) | EBNF §8.2; Symbol §2/§3; Active §Pipe-role boundary/Resources |
| `Q02-BC` | Public, recursive, native/FFI, and typed protocol-boundary callables must source-declare a finite nominal completion-family upper bound. The body actual set must be a subset; every caller statically settles or propagates each unhandled family. This is not a value union or runtime exception contract. | [Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md) §1 and Language Design §5.1.1 | EBNF `callable_contract`; Symbol §2/§7; Active §Core Forms |
| `Q02-SIG` | A non-empty callable completion upper bound is `T ?| {family, family}` with ordinary identifiers and comma separation. `: T` without the clause always means the empty bound; only an eligible non-boundary lexical-local or module-private callable omitting the entire `: T` contract may infer its whole operation summary. `?| {}`, duplicates, non-family names, and trailing commas are rejected. No keyword/token or runtime set is introduced. | [Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md) §1 and Language Design §5.1.1 | EBNF `completion_upper_bound`; Symbol §2/§7; Active §Core Forms/Binding Model |
| `Q02-INF` | Only a capture-safe, final `:=`, non-recursive, non-boundary lexical-local or module-private callable value may receive a definition-site principal constrained rank-1 scheme. Only variables not free in the lexical environment are generalized, and every use is freshly instantiated. First use, future calls, defaults, `any`, and backend choices cannot determine the stable scheme; bare `# f =` only rebinds an established scheme, and required boundaries remain explicit. Closed built-in constraints are supplied by accepted decision `Q05-LIT-ADD`; author-written generics, user instances, higher-rank polymorphism, and completion rows belong to `F02`. Internal notation is not source syntax. | [Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md) | Language Design §5.1.2; EBNF semantic notes; Symbol §2/§7; Active §Core Forms/Binding Model |
| `Q03-F` | Ordinary values are strict/eager without implicit thunks, while independent safe-pure sibling computations have only dependency order and no author-visible left-to-right timeline. Lexical Block items plus accepted data/control/resource edges order observable work. Two unordered order-sensitive siblings in one ordinary expression are rejected and must be prebound/settled or expressed through an existing task construct. `source -> endpoint` makes source value and endpoint capability independent prerequisites of transfer; data direction does not order their preparation. Completion stops later Block items without rollback, publication retains its exit barrier, and reorder/speculate/duplicate/elide use separate proofs. The model lowers through static summaries, DAG/CFG, and no managed runtime. | [Styio Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md) | Language Design §2.4/§8.6; EBNF Block/expression semantic notes; Symbol §2/§5; Active §Functional evaluation and effect order |
| `Q05-LIT-ADD` | Integer and decimal literals remain exact before fail-closed materialization. The compiler-owned `Add` table is closed over `i8`–`i128`, `f32`, and `f64`: concrete operands must be same-type, a literal materializes symmetrically to the other operand, and the result type is unique. Only concrete unconstrained boundaries late-default to `i64`/`f64`; generalized schemes do not default. Integer `Add` is checked and admits payload-free prelude `overflow`; floating `Add` is strict IEEE with no overflow completion. Compile-time/runtime share the relation, a generalized constraint spanning integer and floating rows uses the conservative union `{overflow}`, and known overflow takes that completion edge. No syntax is added; matrix/text/conversion/other arithmetic/remaining NaN questions stay deferred. | [Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md) | Language Design §3.2; EBNF literal semantic note; Symbol §8; Active §Exact numeric literals and `+` |

`D21` freezes direction and composition; `Q01-A` separately freezes transfer
Unit success, an independently valid destination, static completion facts,
settlement matching/payload binding, local evaluation, result joins,
propagation, category separation, discard rejection, and no implicit retry.
Transfer and capture ownership, endpoint construction grammar, chaining,
resource-family escalation, backpressure scheduling, and concurrency policy
remain with their active owner clusters. Q03-F now owns strictness, dependency
order, Block effect sequence, endpoint-preparation ordering, completion stop,
and optimization rights.

## 3. Other accepted source contracts

These contracts were already closed in the active design and therefore are not
owner questions merely because implementation work remains.

| Contract | Accepted result | Normative owner |
|---|---|---|
| Callable binding | `#` marks callable/operable binding; `=` is mutable, `:=` is final; direct and Block bodies remain accepted. | Language Design §5.1; EBNF §4.2 |
| Range spelling | `[start..end]` materializes a range; step-range punctuation is removed; dot runs of length at least two normalize; `x[%n]` is the accepted stride selector. | Language Design §11; EBNF §8/§10; Symbol §4/§5 |
| Resource sessions | `|?|`, `|!|`, and `|>` are the accepted session family; topology declarations remain root-only and session bodies are handles/anchors only. | [Styio-Resource-Topology.md](./Styio-Resource-Topology.md) §4.2 |
| Derived binding | `name := $(deps) => expr` is the accepted frame-commit form; the old head spelling is removed. | Language Design §5.3; EBNF §5 |
| Stream zip | `&` is an event-arrival barrier; snapshot joins are distinct; duplicate external stdin consumption has no implicit tee. | Language Design §10; EBNF §7.1 |
| Break/continue and `>>` | Break and continue are nearest-domain, single-level operations; token length normalizes; multi-role `>>` is compiler-disambiguated by context. | Language Design §6.5/§6.6; Symbol §2/§5 |
| Topology/member syntax | Topology is root-only; explicit writes map driver outputs; `::` defines family members and `.` accesses them. | Styio Resource Topology §4 and Language Design §8 |
| Pressure observer protocol | Latest-wins single-slot level sensor, hysteresis, family-owned watermarks, and `pending/limit/peak` payload are accepted; family activation is implementation/product admission, not syntax design. | Language Design §6.9; Styio Resource Topology |
| Imports | Canonical identity uses slash paths; dot input normalizes; comma/semicolon separators are accepted; trailing separators are rejected. | Language Design §4; EBNF §4.1 |
| Native bindings | Explicit `@ extern(c|c++)` binding lists expose only bound symbols; aggregate-by-value and variadic signatures are not accepted. | Active syntax and native-binding design |
| Series/matrix intrinsic calls | Series and named matrix intrinsics use ordinary call syntax. Current intrinsic/runtime paths do not admit matrix operator rows, mixed-kind coercion, or implicit scalar promotion. | Language Design §3.4/§11.4; StdLib Intrinsics |
| Removed spellings | Infix apply, word-mode selectors, hashtag routing, retired state/history selectors, and first-class dynamic resources are not target syntax. | Active syntax §Rejected Families and owning designs |
| Typed shapes | `T|n|` and `T|..n|` are accepted; untyped `[|n|]` is not canonical. | EBNF §4.2; Symbol §7 |
| Infinite loop | Unconditional `[...] => { ... }` and conditional infinite iteration are distinct accepted forms. | Language Design §6.2/§6.3; EBNF §7 |
| Resource atoms | Explicit `@file`, anonymous `@{...}` / `@(...)`, destroy sink `@()`, and consuming `.close()` forms are accepted. | Language Design §8; Resource Topology |
| Terminal/channel | `[>_]` is canonical terminal handle; `!(expr)` remains the channel selector; statement `>_(expr)` and `(>_)` are compatibility debt, not alternate target design. | Language Design §8.7/§12; Symbol §2/§9 |

## 4. Consequences, not owner questions

The following stay traceable but do not count as unresolved choices:

- Excluding ordinary value fallback eliminates its operand evaluation,
  precedence, chaining, overload, AST/IR, layout, and provenance subquestions.
- Releasing `??`, deleting obsolete task-target routes, renaming AST nodes,
  migrating fixtures, and adding gates are implementation actions.
- Duplicate IDs such as the old sole-expression/tail question, dictionary
  iteration alias, and task-specific arrow premise are ledger tombstones.
- A future feature that is not admitted has one admission gate. Its conditional
  subquestions do not enter the active queue until admission succeeds.

## 5. Explicitly not frozen

No current design document may silently answer these from implementation:

- ownership, transfer, capture, borrowing, view, and capability rules;
- explicit numeric conversion spelling/matrix, unsigned or platform-width
  types, non-`Add` arithmetic and divide-by-zero policy, matrix/text arithmetic,
  and NaN payload/equality/ordering policy beyond `Q05-LIT-ADD`;
- resource-family pressure escalation, buffering, and scheduling policy;
- continuation admission and lifecycle;
- user-defined generic/capability syntax, scoped subtopology, user resource-family
  extensibility, set admission, bitwise syntax, re-export, and selective import.

Their dependency order and recommended decision packets are maintained only in
the owner review until the owner accepts one. Once accepted, the result moves
into its normative design owner and this ledger receives a link; the question is
then removed from the active queue.
