# Styio Language Decision Ledger

**Purpose:** Map every frozen or already accepted language decision to exactly
one normative design owner. This ledger is an index, not a second semantic
specification and not an implementation-status report.

**Last updated:** 2026-07-26

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
| `Q02-BC` | Every callable boundary has one finite nominal completion-family upper bound. An eligible final non-recursive public callable may infer and publish it canonically; recursive, native/FFI, and typed protocol ABI boundaries write it in source. The body actual set is a subset, and every caller settles or propagates each unhandled family. | [Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md) §1 and Language Design §5.1.1 | EBNF `callable_contract`; Symbol §2/§7; Active §Core Forms |
| `Q02-SIG` | A written non-empty completion upper bound is `T ?| {family, family}`. Written `: T` means the empty bound. An eligible callable omitting the entire contract may infer its whole operation summary, including stable public publication. `?| {}`, duplicates, non-family names, and trailing commas are rejected. | [Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md) §1 and Language Design §5.1.1 | EBNF `completion_upper_bound`; Symbol §2/§7; Active §Core Forms/Binding Model |
| `Q02-INF` / `F1-INFERRED-ABSTRACTION` | An eligible capture-safe final non-recursive callable is solved at its definition to one principal constrained rank-1 scheme; public schemes may be serialized into the canonical module interface. Every use is freshly instantiated, and call/import/backend order cannot determine the scheme. Styio has no authored `[T]`, `[Item: type]`, `forall`, or repeated constraint clause. Capability requirements are inferred from body operations; concrete user conformance is explicit and coherent. | [Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md) and [Styio Inferred Abstraction and Explicit Conformance](./Styio-Inferred-Abstraction-and-Explicit-Conformance.md) | Language Design §5.1.2; EBNF semantic notes; Symbol §7; Active §Binding Model |
| `Q03-F` | Ordinary values are strict/eager without implicit thunks, while independent safe-pure sibling computations have only dependency order and no author-visible left-to-right timeline. Lexical Block items plus accepted data/control/resource edges order observable work. Two unordered order-sensitive siblings in one ordinary expression are rejected and must be prebound/settled or expressed through an existing task construct. `source -> endpoint` makes source value and endpoint capability independent prerequisites of transfer; data direction does not order their preparation. Completion stops later Block items without rollback, publication retains its exit barrier, and reorder/speculate/duplicate/elide use separate proofs. The model lowers through static summaries, DAG/CFG, and no managed runtime. | [Styio Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md) | Language Design §2.4/§8.6; EBNF Block/expression semantic notes; Symbol §2/§5; Active §Functional evaluation and effect order |
| `Q04-Core` | Semantic identity—not object size or representation—classifies value copy, affine owner, and lexical borrow/view. `:=`/`=` mutability is orthogonal. Closure/task capture is uniquely inferred from type, use, escape, and capability; only no capture or immutable value snapshots satisfy Q02 `capture_safe`. Borrow cannot escape its owner region. Each endpoint protocol declares one `copy`/`borrow`/`consume` mode and ownership post-state for every exit; committed consume and installed rebinds never roll back. Each untransferred owner contributes one ordered drop/close obligation. Ownership facts enter Q03-F `EvaluationFacts`/DAG and never `OperationSummary`. No ownership or capture-list syntax is added. | [Styio Ownership, Capture, and Capability](./Styio-Ownership-Capture-and-Capability.md) | Language Design §5.4/§8.5–§8.6; EBNF semantic notes; Symbol §1/§2/§6/§7; Active §Ownership, capture, and capability |
| `Q05-LIT-ADD` | Retained literal subset: integer and decimal terms remain exact before fail-closed contextual materialization; only concrete unconstrained boundaries late-default to `i64`/`f64`, and eligible generalized schemes do not default. The former same-type `Add` rows are superseded by `Q05-NUMERIC-OPS`; this owner now defines no operator relation. | [Styio Exact Numeric Literals](./Styio-Exact-Literals-and-Builtin-Add.md) | Language Design §3.2; EBNF literal semantic note; Active §Exact numeric literals |
| `Q05-SCALAR-CONV` | `expr :> T` is the only author-visible checked conversion for `i8`–`i128`, `u8`–`u128`, `f32`, and `f64`. `:>` is contiguous and non-associative; the right identifier resolves in type context without becoming callable. Every source/target row has a precise subset of `out_of_range`, `inexact`, and `non_finite`. Total lossless `Widen`, heterogeneous operator rows, and exact literal materialization remain distinct. | [Styio Checked Scalar Conversion](./Styio-Checked-Scalar-Conversion.md) | Language Design §3.2.1; EBNF `conversion_expr`; Symbol §8/§10; Active §Checked scalar conversion |
| `Q05-INT-DIVREM` | Retained invariant subset: signed-integer `/` and `%` widen both operands to `WiderInt(I,J)` and return that type. For nonzero `b`, they produce the unique Euclidean `q`, `r` satisfying `a=q*b+r` and `0<=r<abs(b)`; `/` has `{divide_by_zero, overflow}`, `%` has `{divide_by_zero}`, and `MIN_W%-1` succeeds with `0` even though `MIN_W/-1` completes `overflow`. The unified numeric owner supplies operand/result rows and compound integration; this owner preserves the invariant and exceptional values. | [Styio Euclidean Signed-Integer Division and Remainder](./Styio-Euclidean-Signed-Integer-Division-and-Remainder.md) | Language Design §3.2.2; EBNF multiplicative semantic note; Symbol §8; Active §Built-in numeric operators |
| `Q05-NUMERIC-OPS` | The closed scalar domain is `i8`–`i128`, `u8`–`u128`, `f32`, and `f64`; there are no platform-width integer identities or scalar `byte`. Value flow uses only total lossless widening. Same-signedness arithmetic widens by width; mixed signed/unsigned arithmetic uses the smallest complete signed domain and has no row when none exists (notably signed with `u128`). Every numeric pair compares exactly. Mixed float arithmetic rounds once; checked integer, Euclidean signed division, unsigned quotient/remainder, strict IEEE rows, completion sets, and transactional compounds follow the focused owner. | [Styio Built-in Numeric Operators and Inference](./Styio-Builtin-Numeric-Operators-and-Inference.md) | Language Design §3.2.2; EBNF §8; Symbol §6/§8/§9; Active §Built-in numeric operators and inference |
| `Q06-TEXT-BINARY` | `scalar` is one Unicode scalar; `char` is one extended grapheme cluster; `string` is valid length-aware UTF-8 with embedded NUL and grapheme-default indexing. Exact scalar-sequence equality performs no normalization; `canon_eq`/NFC/NFD are explicit library operations. Identifiers use XID+NFC with fail-closed invisible/confusable checks. `bytes`, `bits`, and `blob` are ordinary optional types; `u8` is the octet and scalar `byte` is removed. Codec/decode facilities are explicit standard-library modules, not prelude. | [Styio Unicode Text and Binary Values](./Styio-Unicode-Text-and-Binary.md) | Language Design §3.1/§3.6; EBNF lexical/type notes; Active §Types |
| `D1-DATA` | Tuples are structural ordered products; declared records and variants are nominal. Construction and patterns preserve identity, owner/borrow facts propagate without implicit copy/discard, closed variant matches are exhaustive, and FFI/layout adaptation is explicit. | [Styio Data and Collection Model](./Styio-Data-and-Collection-Model.md) §1–§2 | Language Design §3.6; EBNF §11; Active §Types |
| `D2-COLLECTIONS` | Materialized collections use recursive value semantics and deterministic order. Ordinary slices are stable value snapshots; explicit views borrow. Iterators/streams are distinct protocol values, and only obligation-bearing cursors/subscriptions are affine. `list[T]` is not `T..`; codec/decode remains outside prelude. | [Styio Data and Collection Model](./Styio-Data-and-Collection-Model.md) §3–§6 | Language Design §3.6/§9.3; EBNF §10; Active §Types |
| `D3-RESOURCES` | Capability, typestate, ownership kind, and exit obligation are orthogonal. Obligation-bearing handles/tasks are affine; value streams/snapshots may be values. Tasks are structured with no v1 detach; cancellation must be followed by join before release. EOF/absence/failure/cancel/shutdown/pressure remain distinct; async edges have finite explicit backpressure and never hidden unbounded queues or implicit broadcast. | [Styio Structured Resources and Concurrency](./Styio-Structured-Resources-and-Concurrency.md) | Language Design §6.9/§8; EBNF §7/§9; Active §Resources/Tasks |
| `D4-MODULES` | Canonical identity is resolved package source/name/version plus slash module path, namespace, and declaration. Imports bind module namespaces; selective import, alias, export, and re-export are explicit; default visibility is private; glob/dot-path/semicolon compatibility is removed. V1 rejects module cycles and implicit effectful top-level initialization. Resolution and protocol implementations are globally coherent and order-independent; prelude stays minimal. | [Styio Module and Extension Model](./Styio-Module-and-Extension-Model.md) | Language Design §4; EBNF §4.1; Active §Core Forms |

`D21` freezes direction and composition; `Q01-A` separately freezes transfer
Unit success, an independently valid destination, static completion facts,
settlement matching/payload binding, local evaluation, result joins,
propagation, category separation, discard rejection, and no implicit retry.
Q04-Core now freezes transfer/capture ownership, endpoint mode/post-state, and
drop facts without changing direction. D3 now owns the conservative structured
lifecycle and bounded-backpressure baseline; concrete family capacities,
escalation thresholds, and scheduling algorithms remain family contracts.
Endpoint construction grammar and chaining remain with their focused owners.
Q03-F owns strictness,
dependency order, Block effect sequence, endpoint-preparation ordering,
completion stop, and optimization rights.

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
| Imports and exports | Slash paths are canonical and exclusive. Module import, selective import, alias, explicit export/re-export, comma lists, and trailing commas follow `D4-MODULES`; dot paths, semicolon separators, and glob imports are rejected. | [Styio Module and Extension Model](./Styio-Module-and-Extension-Model.md); Language Design §4; EBNF §4.1 |
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

- intentionally lossy numeric round/truncate/saturate/wrap operations,
  platform-width types, matrix/text arithmetic, and NaN payload/total-order/hash
  policy beyond the accepted Q05 subdecisions;
- resource-family-specific pressure escalation, buffer sizes, drop policies,
  and scheduling algorithms beyond the accepted bounded D3 contract;
- continuation admission and lifecycle;
- user-defined operator participation, conversions, associated types, dynamic
  dispatch, scoped subtopology, set-specific equality/hash policy, and bitwise
  syntax;
- macro/compile-time generation, raw-pointer/unsafe boundaries, reflection,
  dynamic loading, higher-rank/higher-kinded polymorphism, and detached tasks.

Their dependency order and recommended decision packets are maintained only in
the owner review until the owner accepts one. Once accepted, the result moves
into its normative design owner and this ledger receives a link; the question is
then removed from the active queue.
