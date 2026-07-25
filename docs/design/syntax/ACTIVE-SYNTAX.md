# Active Syntax Map

**Purpose:** Provide the compact authoring map for current Styio syntax; grammar authority stays in [../Styio-EBNF.md](../Styio-EBNF.md), token authority stays in [../Styio-Symbol-Reference.md](../Styio-Symbol-Reference.md), and semantics stay in the owning design documents.

**Last updated:** 2026-07-20

## Reading Contract

1. This page lists canonical authoring forms. Inclusion is a design fact; it
   does not establish that parser, Sema, lowering, and runtime support are complete.
2. Implementation availability and compatibility deletion are separate axes
   recorded by the convergence matrix, rollups, and Better Plans. Parser
   recognition alone never promotes a form into this map.
3. It is not a catalog of retired syntax. Historical spellings are recovered
   from Git history only when needed for migration archaeology.
4. Positive examples in `example/` and `tests/` should use the canonical forms
   below unless a test is explicitly a compatibility or negative migration fixture.

## Core Forms

| Area | Canonical form | Owner |
|------|----------------|-------|
| Imports | `@import { pkg/module }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Final binding | `name [: T] := expr` | RHS required; [../Styio-Language-Design.md](../Styio-Language-Design.md) |
| Mutable binding | `name [: T] = expr` | RHS required; [../Styio-Language-Design.md](../Styio-Language-Design.md) |
| Callable binding | pure `# name : T := (arg: U) => { ... }`; bounded `# name : T ?\| {io, parse} := (arg: U) => { ... }`; inferred eligible final local/private `# name := (arg) => expr` | [../Styio-Callable-Principal-Inference.md](../Styio-Callable-Principal-Inference.md) |
| Match sugar | `#(name = expr) ?= { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Lexical Block result | `<| expr` | Completes only the current Block; [BLOCK_COMPLETION.md](./BLOCK_COMPLETION.md) |
| Inline lexical Block result | `|<| expr |;` | Same node/target; `|;` is mandatory; [BLOCK_COMPLETION.md](./BLOCK_COMPLETION.md) |
| Conditional | `?(cond) => { ... } | { ... }` | The `|` is the else-branch delimiter anchored by the preceding `?(cond) =>`; it is not a binary value operator. [../Styio-EBNF.md](../Styio-EBNF.md) |
| Materialized range | `[start..end]` | Canonical iterable/source form. The bracketed form materializes the expression-level range `start..end` as `list[i64]`; it is not a list literal containing one range expression. `[start..end] >> #(x) => { ... }` advances the range one item at a time and pushes each item into the consumer. Step ranges are removed from the design and rejected. |
| Iteration / pulse transfer | `iterable >> #(x) => { ... }` | `>>` advances the left iterable or stream one item at a time and pushes each item as a pulse into the right closure/channel. Multi-role `>>` (pipe / resource write / continue) is a settled design requirement; disambiguation is compiler-owned. |
| Infinite stream loop | `[...] >> ?(cond) => { ... }` | [../Styio-EBNF.md](../Styio-EBNF.md) |
| Continue | `>>`, `>>>`, `>>>>`, ... as a standalone statement | Skip the rest of the current block for this pulse/session and resume at the next pulse/session of the nearest continue-capable domain; token length is ignored. Break and continue are single-level only by settled decision (goto-hell prevention); multi-level jumps are permanently rejected. |

### Block result model

`=> expr`, `=> { expr }`, and `=> { <| expr }` are equivalent only when the
Block body has exactly one expression item. Multi-item Blocks do not infer a
tail result and use `<|` explicitly for a non-Unit value. Reaching `}` normally
produces `() : unit`; reachable `T` and Unit exits are incompatible, while a
proven `never` edge contributes no normal value. `<|` never crosses a lexical
Block boundary, and only the outer function-body Block result becomes a
function result. Unit-only consumers reject non-Unit yields; structurally
unreachable siblings are compile-time errors.

### Functional evaluation and effect order

Ordinary calls, operators, composites, and indexing use strict values without
implicit thunks. Independent sibling computations proven pure, total,
completion-free, and resource/identity-unobservable have only dependency order;
source position does not create a left-to-right time edge. Top-level Block
items, data/control edges, `?|` selection, and resource happens-before edges
order observable work. Two unordered order-sensitive siblings in one ordinary
expression are a compile error: settle/bind them in consecutive Block items, or
use an existing task construct when concurrency is intended. No syntax is added.

Full contract:
[Functional Evaluation and Effect Ordering](../Styio-Functional-Evaluation-and-Effect-Ordering.md).

### Binding Model

`=` is the mutable binding operator. It may bind or rebind an ordinary value with
`name = expr`. Under `#`, an initial mutable callable needs a complete explicit
contract; a bare `# name = (...) => expr` or `# name = #(args) => expr` only
rebinds an already established stable scheme and cannot infer or change it.

`:=` is the final binding operator. It may bind an ordinary final value with
`name := expr`, and it may bind a final callable or operation-channel endpoint
with `# name := (...) => expr` or `# name := #(args) => expr`. A final callable
binding cannot later be replaced by `=` or another `:=`. Only an otherwise
eligible final callable value may receive automatic principal generalization.

An ordinary binding always includes its RHS. Bare `name : T` is rejected for
every ordinary type, including `? | T` and `unit`; explicit empty and Unit
values are written `(?)` and `()`. The compiler never inserts either value and
never manufactures zero, `false`, an empty string, uninitialized storage, or an
implicit default. Parameters and pattern/iteration binders are not empty
declarations because their enclosing operation supplies the first value. Schema
and resource-topology declarations keep their own construction/protocol rules
and gain no default from this distinction. Settlement results use an ordinary
binding RHS, for example `answer: T = ?| operation | recovery`; settlement does
not declare a typed target.

`#` marks a binding as callable/operable; it is not a resource prefix and not a
plain `def` keyword. The right side of a `#` binding must be a callable body or
operation-channel body. Resource identities remain visibly in the `@` family, so
`# sink = @stdout` is invalid; use `expr -> @stdout`, `items >> @stdout`, or a
resource-family declaration when the target is a resource.

A callable completion contract follows its normal result type as
`T ?| {family, family}`. The clause is a finite non-empty upper bound of nominal
family identifiers; braces and commas are signature structure rather than a
runtime set/dict, Block, value union, fallback, or handler. `: T` without the
clause always asserts an empty bound in every scope. Only an eligible
non-boundary lexical-local or module-private callable omitting the entire `: T`
contract may infer its whole operation summary. `?| {}`, duplicate/non-family
names, and trailing commas are rejected. Public/exported, recursive, native/FFI,
and typed protocol-boundary callables must write every parameter type and the
contract.

The detailed `Q02-INF` rule is owned by
[Styio Callable Principal Inference](../Styio-Callable-Principal-Inference.md).
Its compact invariant is: capture-safe, final `:=`, non-recursive,
non-boundary local/private callable values are solved to a definition-site
principal constrained rank-1 scheme; only variables not free in the lexical
environment are generalized, and every use receives a fresh instance. First
use, future calls, defaults, `any`, and backend choices cannot determine the
scheme. Internal `forall`/literal/`Add` notation is not source: accepted
`Q05-LIT-ADD` supplies the closed built-in contract through
[Styio Exact Literals and Built-in Add](../Styio-Exact-Literals-and-Builtin-Add.md),
and `F02` owns future author-written generic or completion-row syntax.

## Types

| Shape | Meaning |
|-------|---------|
| `unit` / `()` | first-class one-value Unit type and its unique value; zero payload does not erase semantic state |
| `never` | contextual built-in bottom type for proven non-completion; no value/default or keyword token; `join(T, never) = T` |
| `? \| T` | optional union; `(?)`, `[?]`, and `{?}` are the same empty value, a `T` expression is present, and `? \| (? \| T)` normalizes to `? \| T` |
| `T|n|` | exact-length resource shape |
| `T|..n|` | recent-window resource shape |
| `T..` / `T...` | unbounded sequence type |
| `list[T]` | type rewrite to `T..` |
| `dict[K, V]` | type rewrite to `(K, V)..` |
| `(A, B, C)` | tuple type |
| `__ : Pattern := Type` | type rewrite declaration in type position |

### Exact numeric literals and `+`

Canonical forms include:

```styio
small: i8 = 5
count := 5
# add_five := (x) => x + 5
# add_i64 : i64 ?| {overflow} := (a: i64, b: i64) => a + b
safe: i64 = ?| add_i64(left, right) | overflow => 0
```

Integer and decimal literals remain exact until context materializes them.
Unconstrained concrete boundaries default once to `i64` and `f64`; a
generalizable callable scheme never applies that default. Built-in scalar `+`
is closed over `i8` through `i128`, `f32`, and `f64`: concrete operands must be
the same type, a literal can materialize symmetrically to the other operand,
and the result is that type. Integer addition is checked and admits the
payload-free ordinary completion identifier `overflow`; floating addition uses
strict IEEE behavior and has no `overflow` completion. A generalized `Add`
scheme spanning integer and floating rows uses the finite conservative bound
`{overflow}`, including its floating instantiations.

The full semantics are owned by
[Styio Exact Literals and Built-in Add](../Styio-Exact-Literals-and-Builtin-Add.md).
This adds no author-visible constraint syntax or grammar form. Matrix/text `+`,
mixed concrete conversion, other arithmetic, and remaining NaN policy are not
admitted by this entry.

`unit` is a normal argument to `? | T`, `list[T]`, `dict[K,V]`, and
`task[T]`. `? | unit` keeps an explicit presence state; `list[unit]` keeps
logical length and iteration count; `dict[K,unit]` keeps key membership; and
`task[unit]` keeps lifecycle/failure state. Implementations may erase Unit
payload bytes but must never infer these logical facts from bytes, allocation,
addresses, or an integer placeholder. Foreign `void`, declared nullable values,
and proven no-return calls enter only through explicit adapters as `unit`,
`? | T`, and `never`; unspecified pointer nullability fails closed.

### Pipe-role boundary

`|` is not a general binary value operator. A general expression such as
`a | b`, `true | false`, or `0 | 1` is rejected by the parser; operand types,
purity, or later inference never reinterpret it as Optional recovery. The
remaining pipe-shaped roles are selected by their enclosing grammar before
type inference:

| Context | Accepted role |
|---------|---------------|
| Type position | `? | T` forms the optional union. |
| Callable result contract | Contiguous `?| {io, parse}` after `: T` introduces a comma-separated finite completion upper bound; it consumes no bare `|`. |
| Conditional guard | `?(cond) => when_true | when_false` uses `|` as the guard-anchored else delimiter. |
| Operation settlement | A leading `?| operation | recovery`, `?| operation | family => recovery`, or `?| operation | family(binding) => recovery` uses later `|` tokens only as branches of that settlement construct. |

Styio deliberately has no ordinary value-level fallback or coalescing
operator. Both `value | default` and `value ?? default` are syntax errors; this
boundary is decided before operand typing, effect analysis, or purity analysis.
`??` has no accepted syntax role. The implementation must delete its legacy
token and former speculative diagnostic-extraction path rather than reserve or
repurpose either one. Optional values use explicit control flow; they are never
silently converted into an ordinary fallback expression.

The leading settlement form `?| operation | recovery` is intentionally
different. It wraps one complete resource/task/effect operation and does not
restore a general value operator. Every operation has one static success type
and a finite nominal completion-family set. Success bypasses recovery; only the
selected arm runs lazily once, with no implicit retry. Named arms match exact
families; `family(binding)` binds a typed payload. Both names are ordinary
identifiers, not keywords. A final bare fallback catches only remaining
recoverable failures. All normal results join with the success type; unhandled
families propagate. `?| operation | ...` is rejected.

If the operation or selected recovery completes instead of returning normally,
later items in the current Block do not start. There is no implicit rollback;
mandatory lexical exit obligations still run before any candidate Block result
is published. The complete stop/publication contract is owned by
[Functional Evaluation and Effect Ordering](../Styio-Functional-Evaluation-and-Effect-Ordering.md)
and [Block Completion](./BLOCK_COMPLETION.md).

### Directional transfer axiom

`left -> right` always depicts one direction of real data movement: the value
produced at the left location/action flows into the
destination/location/receiver endpoint on the right. Assignment, export,
resource write, channel send, and task-result delivery are not separate arrow
roles. Each endpoint's separately decided protocol determines compatibility and
lowering without changing the arrow's meaning. The destination never declares a
name and must independently be a writable endpoint. Successful transfer is
`unit`; it does not yield the source, destination, or an implicit receipt.
Source value and endpoint capability are independent prerequisites; the data
direction does not imply source-before-endpoint preparation. Two
order-sensitive preparations must first be sequenced as Block items. This axiom
does not activate or define chained arrows, associativity, ownership, or
backpressure scheduling.

Settlement is orthogonal. `?| source -> destination | recovery` parses as
`?| (source -> destination) | recovery` and settles the complete transfer
operation. It is never a task-only binder. To bind settlement's returned value,
write `answer = ?| operation | recovery`. The shapes
`?| source -> name: T` and `?| -> name: T` are rejected.

## Resources

| Area | Canonical form | Notes |
|------|----------------|-------|
| Resource slot | `@price : f64|..10|` | Top-level only; local-block declarations — including inside `|?|` sessions — are rejected. Resource sessions (`|?| { ... }`, Resource Topology §4.2) authorize handles and anchors only. Scoped subtopology remains a separate fail-closed reserve; resources as first-class dynamic values are permanently rejected. |
| Resource session | mid-transfer `=> \|?\| { ... } \|!\|(cleanup) =>` / statement `?\| \|?\| { ... } \|> ... \|> cleanup =>` | Explicit handle/anchor session (Resource Topology §4.2). No `session` keyword. `|?|` only mid-transfer (symbols before and after); `?|` opens statement settlement. Default Close; `|!|(cleanup)` special exit; `|>` defers settlement. Escape: static reject move-out and join owned `||>` tasks. Continuation behavior is not predeclared by this surface. |
| Multi-resource slot | `@a : f64|..2|, @b : f64|..2| := { ... }` | Driver-block coverage remains staged by topology tests. |
| Resource-endpoint transfer | `expr -> @price` | An instance of generic directional `->`: the resource endpoint makes this edge a pending resource write during lowering; it does not create a resource-specific arrow role. Success is `() : unit`; resource failures remain static completion families. |
| Resource block | `resource >> { ... }` / `resource >> #(x) => { ... }` | Iterates the resource one produced item at a time into the block, enters a snapshot at `>>`, and commits snapshot result at `}`. Chained block stages commit once per block. |
| Latest read | `@price[-1]` | Reads committed resource state or the current block snapshot. |
| Slice read | `@price[-3..]`, `@price[...]` | Resource-object selectors; bounded selector snapshots materialize iterable lists, while scalar latest reads remain non-iterable. |
| Stdin pull | `value <- @stdin` | Untyped scalar pull. |
| Typed stdin pull | `a, b <- @stdin : (f64, f64)` | Tuple/list/scalar forms share the stdin-pull path. |
| Stdin iteration | `@stdin >> #(line) => { ... }` | `@stdin` is read-only for data flow; each input line is pushed as one pulse into `line`; explicit resource operations may still release it. |
| Stdin zip source | `@stdin >> #(line) & xs >> #(x) => { ... }` | Finite zip is accepted with materialized lists or `@file` streams; duplicate `@stdin & @stdin` remains unsupported until the stream-driver contract defines duplicate consumption. |
| Stdout/stderr scalar write | `expr -> @stdout`, `expr -> @stderr` | `@stdout` and `@stderr` are write-only data sinks; explicit resource operations may still release them. |
| Resource-sink iterable write | `items >> @stdout`, `items >> @stderr`, `items >> @file("out.txt")` | Each iterable item is serialized and written as a sink pulse. Plain strings should use `->` unless explicitly split. |
| File resource | `@("log.txt")`, `@file("log.txt")` | Runtime substrate is file-backed when resolved as a file. |
| Empty resource sink | `@()` | Destroy sink / empty resource. |
| Explicit copy | `snapshot << @price[...]`, `copy << list_or_dict_or_matrix` | Selector snapshots and materialized list/dict/matrix handles copy through `<<`; `<-` stays resource acquire / task pull, not bound-resource clone. |
| Operation settlement | `?\| operation \| recovery` | Settles one complete operation. Success returns its static success type. Only the selected recovery runs lazily once; all normal arms join to the success type; remaining completion families propagate. A trailing catch-all catches recoverable failures only. An inner `->` remains a directional transfer. Bare `value \| recovery` and `?\| op \| ...` are syntax errors. |
| Named settlement arm | `?\| read() \| io(problem) => recover(problem)` | `io` is a semantically resolved completion-family identifier and `problem` is a branch-local payload binding; neither is a keyword. The bare `io => recovery` form matches the same exact family without binding. Duplicate families are rejected and catch-all must be last. Pressure becomes matchable only after its resource protocol escalates it into a nominal failure family. |
| Pressure observer | `channel.pressure >> #(p) => { ... }` | Optional resource-family effect stream for observing non-failure pressure such as backlog growth. Contract: single-slot conflated latest-wins delivery (no reading queue, no meta-pressure); pulses only on hysteresis state transitions (enter/exit/escalate); payload is a prelude read-only struct (`pending`/`limit`/`peak`) pending the general struct story; observer bodies run off the writer path; observer-write cycles, duplicate observers, non-module-scope observers, and zip use are rejected. No current family exposes the stream; unsupported families fail closed in Sema with `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`. |

## Resource Family Members

| Area | Canonical form | Notes |
|------|----------------|-------|
| Mutable method binding | `@file::close = () => { @file -> @() }` | Standard-library bindings may be overridden. |
| Final method binding | `@file::close := () => { @file -> @() }` | Final binding cannot be overridden. |
| Property binding | `@file::path := expr` | Property access is not callable. |
| Method call | `log.close()` | Consuming methods invalidate the receiver immediately. |
| Direct resource call | `@("log.txt").close()` | Late resource construction is still statically resolved before lowering. |
| Receiver reference | `@file` / `@file.path` inside `@file::name` body | Refers to the receiver instance, not a constructor; receiver postfix property/method access is scoped to the resource-family definition body. |
| Statement-preface return | `@file::answer = () => { >_("inside") <\| 42 }` | Value-returning method bodies may run accepted statement-only prefaces before a final returned expression. Scalar local `=` or `:=` prefaces such as `x = 41; <\| x + 1`, plus local list/dict/matrix `=` or `:=` prefaces such as `xs = [40, 2]; <\| xs`, `m: matrix := [[1, 2]]; <\| m[0][0]`, or `m: matrix := [[1, 2]]; <\| m`, are scoped to the inlined method body when the final return is scalar/string or the local container value; local resource bindings and capture-dependent bodies remain fail-closed. |
| Returned resource effect | `@file::read_or = () => { <\| ?\| (<< @file("data.txt")) \| 7 }` | The operation success and fallback must join to the method's result type. Current compiler coverage remains implementation-specific, but the completion algebra is fixed. Settlement discard has no canonical source form. |
| Returned match expression | `@file::pick = (x: int) => { <\| x ?= { 0 => 'a' _ => 'b' } }` | Single-return method bodies may return match expressions for the current scalar/string result families; container match results remain fail-closed. |
| Returned function call | `@file::score = (x: i64) => { <\| plus_one(x) }` | Single-return method bodies may return calls to ordinary functions when the called function has a value tail or explicit `<\| expr`; statement-only function bodies remain fail-closed as method return values. |

## Tasks

| Area | Canonical form | Notes |
|------|----------------|-------|
| Single task | `job = ||> { ... }` | Produces a task handle. |
| Task group | `||> [ t1 := { ... } t2 := { ... } ]` | Each entry binds a task handle. |
| Task settlement | `answer: T = ?\| job \| recovery` | Tasks use the same operation settlement as every other effectful operation. `job -> answer` remains a generic directional transfer when the endpoints are compatible; `?\| job -> answer \| recovery` settles that complete transfer. There is no task-only `?\| ... -> name: T` binder. |
| Resource order | `t1 => t2` | Explicit happens-before edge; it does not transfer data. |

## Rejected Families

The active syntax docs name rejected families only as migration boundaries:

1. Retired state-resource containers and state references are parser errors.
2. Source-level bare `@` is not an authoring form.
3. Reserved wave tokens `<~` and `~>` are reserved symbols only: they participate in no syntax feature until the language design explicitly declares an activation, and every use remains fail-closed.
4. The infix apply-pipe spelling (`f <| a <| b`) is removed; ordinary application uses call syntax `f(a)(b)`. This decision does not activate continuation syntax.
5. Word-mode selectors (`x[avg, n]`, `x[max, n]`, and the deferred `[min/std/ema/rsi, n]` family) are removed: selectors are a pure-symbol algebra and no identifier participates in selector syntax. Series intrinsics use ordinary call syntax `avg(x, n)` / `max(x, n)` recognized in Sema; the parser bracket path is compatibility debt. The stride selector `x[%n]` is active and keeps positions whose zero-based index is congruent to 0 modulo positive integer `n`; literal non-positive strides are rejected statically and dynamic non-positive strides fail through the runtime error channel.
6. The old reactive-capture head spelling `name $(deps) := expr` is removed. The accepted derived-binding surface is `name := $(deps) => expr` with frame-commit semantics and the strict fail-closed whitelist in Language Design §5.3; it is parser-pending and fails closed.
7. Ordinary value-level fallback/coalescing is rejected at the syntax boundary.
   `a | b`, `a ?? b`, `true | false`, and `0 | 1` never enter type- or
   purity-directed recovery. `??` has no accepted role; its legacy token and
   former diagnostic-extraction use are implementation-removal debt, not a
   reservation. This does not remove type `? | T`, the guard-anchored else
   delimiter, or recovery branches inside a leading `?|` effect/resource/task
   settlement.
8. The task-only await/binder spellings `?| job -> name: T` and
   `?| -> name: T` are rejected. This does not reject generic `job -> name` or
   settlement of a generic transfer as `?| job -> name | recovery`.
9. Compatibility spellings that remain implemented must not be used as the default teaching surface.
