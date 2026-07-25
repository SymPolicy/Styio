# Styio Functional Evaluation and Effect Ordering

**Purpose:** Define the accepted Q03-F semantics for strict value evaluation,
dependency-only pure computation, explicit effect/completion sequencing, Block
stop edges, directional-transfer prerequisites, and optimization rights.

**Last updated:** 2026-07-20

**Status:** Design accepted; implementation pending. This document is the
unique focused semantic owner for Q03-F. Current parser, AST traversal, Sema,
SGIR, LLVM emission, runtime error state, tests, or optimizer behavior are not
language authority.

**Implementation owner:** [Styio Functional Evaluation and Effect Ordering
Plan](../plan/Styio-Functional-Evaluation-and-Effect-Ordering-Plan.md).

## 1. Accepted model

Styio uses **strict values + dependency graphs + explicit effect sequences**.
These are three separate rules:

1. Ordinary application, operator, composite construction, and indexing are
   eager/call-by-value. Their strict inputs must become values before the
   parent operation starts. They do not create implicit thunks or call-by-need
   update cells.
2. Independent sibling computations proven pure, total, completion-free, and
   resource/identity-unobservable have no author-visible source-order timeline.
   They are constrained only by data and control dependencies.
3. Observable effects, completions, and non-normal-return possibilities are
   ordered only by explicit data/control/resource edges and lexical Block-item
   sequence. An ordinary expression with two unordered order-sensitive sibling
   computations is rejected statically.

This separation preserves the language's “Everything Is a Flow” model without
copying the unsafe half of languages that leave effectful operand order
unspecified. It adds no token, keyword, operator, value wrapper, monad, handler,
or runtime object.

## 2. Terms

### 2.1 Computation node

A computation node is one typed evaluation unit in the compiler's operation
graph. Q01-A remains the unique owner of `OperationSummary`; Q03-F composes it
with separate evaluation facts instead of adding fields to it:

```text
OperationSummary = {
    success_type,
    completion_set
}

EvaluationFacts = {
    operation: OperationSummary,
    effect_accesses,
    normal_return_fact,
    totality_fact,
    optimization_rights
}
```

`effect_accesses`, `normal_return_fact`, `totality_fact`, and
`optimization_rights` are Q03-F facts; they neither duplicate nor mutate the
success/completion algebra. The notation is descriptive compiler terminology,
not Styio source syntax.
Completion families remain the finite nominal identities accepted by Q01-A.

### 2.2 Safe pure node

A node belongs to the freely schedulable pure value graph only when the
compiler proves all of the following for the relevant transformation:

- it has no externally observable or resource effect;
- its completion set is empty;
- it returns normally and is total for the admitted input facts;
- it does not expose allocation, address, identity, version, volatile, task,
  stream-consumption, or resource-state observations;
- the requested optimization preserves the same value and required numerical
  semantics.

`pure` alone is insufficient. A pure-looking computation may still overflow,
trap, fail allocation, diverge, consume a stream position, observe mutable
identity, or otherwise prevent safe speculation or deletion.

### 2.3 Order-sensitive node

A node is order-sensitive when moving it relative to another observable node
can change the program's value, completion, external actions, resource state,
normal-return behavior, or publication point. The conservative set includes:

- resource, I/O, task, native/FFI, control, consume, commit, drop, close, or
  other externally observable effects;
- a non-empty finite completion set;
- explicit `never` or another proven no-normal-return path;
- a call whose total/normal-return fact is not proven;
- an Unknown/native summary or an access whose alias, version, or capability
  relation is not proven safe.

Unknown information is the conservative top fact. The compiler never turns an
unknown node into a safe pure node because the current backend appears harmless.

### 2.4 Explicit order edge

An explicit order edge is one already justified by the program or an accepted
construct:

- producer-to-consumer data dependency;
- control/lazy dependency of a conditional, match, short-circuit operation, or
  settlement selection;
- lexical order between top-level items of one Block;
- resource-topology happens-before, ownership/drop, commit, consume,
  backpressure, or capability dependency;
- task settlement/join or another separately accepted concurrency edge.

The source position of ordinary sibling operands is not an explicit order edge.

## 3. Strict evaluation without a left-to-right timeline

### 3.1 Parent readiness

For an ordinary strict parent node `P` with required inputs `c1 ... cn`:

```text
c1 --data--> P
c2 --data--> P
...
cn --data--> P
```

`P` starts only after every required child has produced a normal value. The
edges do not order independent children against each other. The compiler may
choose any stable topological order and may select a different physical order
after a semantics-preserving optimization. Lack of a source order does not
authorize automatic task creation or concurrent execution.

### 3.2 Exact-once source meaning

Each evaluated source expression denotes one logical evaluation at its use
site. Inlining a callable must first bind each argument value once; repeated
parameter references do not repeat the argument computation.

Physical duplication, commoning, speculation, or elimination is permitted only
when the corresponding optimization right in §10 is proven. This is the as-if
boundary between author semantics and generated instructions.

### 3.3 One order-sensitive child

One order-sensitive child plus any number of safe pure siblings is not an
ordering ambiguity. Safe pure siblings have no observable event relative to
that child. If the child completes instead of returning a value, the parent
does not run or publish a result.

### 3.4 Two unordered order-sensitive children

An ordinary parent with two or more order-sensitive children that are not
ordered by data, control, resource, or other accepted edges is ill-formed.
Sema reports the conflicting child source ranges and the missing ordering fact.
It must not choose left-to-right, right-to-left, pointer order, hash order,
backend order, scheduler race, or “first completion observed”.

The author resolves the ambiguity by:

1. placing the computations in consecutive Block items and settling/binding
   their values before composition; or
2. using an existing task/concurrency construct when concurrent execution is
   intended, then settling its result through the accepted operation model.

No new `do`, `seq`, monad, handler, or ordering operator is introduced.

## 4. Block sequence and completion stop

### 4.1 Lexical sequence

Top-level items of one lexical Block establish an order-sensitive
happens-before sequence. A later order-sensitive item does not start until the
earlier item has normally settled. A value dependency may impose the same edge
independently.

Safe pure work with no data dependency may move across a lexical boundary under
the as-if rules because it has no author-visible event. This does not weaken
the order of effects, completions, commits, drops, or publication.

Example:

```styio
first := ?| read_first()
         | recover_first()
second := ?| read_second()
          | recover_second()
<| combine(first, second)
```

The first operation settles before the second starts. `combine` receives two
ordinary prepared values; its two argument reads need no time order.

### 4.2 Completion stop

When a Block item completes without a normal value:

- later ordinary Block items do not start;
- the current candidate result is not published;
- the completion propagates or is settled according to Q01-A;
- all already registered mandatory exit obligations still run through the
  accepted Block-exit dependency graph;
- previously completed external effects are not implicitly rolled back.

This is a static control-flow edge, not dynamic exception-stack search.

### 4.3 Publication remains separate

Candidate readiness precedes publication. A Block result is published only
after required exit obligations settle, as already defined by Lexical Block
Completion. Q03-F does not reopen the candidate/exit/publication decision or
turn it into a transaction.

### 4.4 Specialized Blocks

The following retain their focused semantics rather than inheriting an
accidental generic rule:

- a derived binding recomputes by its static frame dependency graph;
- a task group expresses concurrency explicitly;
- a resource-entering Block uses its snapshot/commit boundaries;
- a stream/pulse Block follows its accepted per-pulse demand model.

Their order-sensitive operations still contribute edges to the unified graph.

## 5. Operators and calls

### 5.1 Ordinary operators

Both operands of an ordinary binary operator are strict prerequisites. Neither
source position orders the other operand. The operator itself runs only after
both values exist.

For accepted checked integer `Add`, range checking occurs at the Add node after
both input values are ready. An `overflow` completion publishes no numeric
result. If both operand computations are themselves order-sensitive and no
edge orders them, the expression is rejected before lowering.

### 5.2 Callable application

The callee/receiver and every strict argument are prerequisites of the call
body. Their sibling positions do not establish time order. The body starts
exactly once only after all prerequisites return normally.

An implementation may not substitute an argument AST independently at every
parameter occurrence. It must materialize one argument evaluation fact/value
and let parameter reads consume that value.

### 5.3 Composite construction

Tuple, list, record, dictionary, and later admitted composite elements are
strict value prerequisites unless their own focused design declares a lazy
construct. Source element order does not establish time order. The outer value
is published only after every required element succeeds; partial storage is
not observable.

Two unordered order-sensitive elements, fields, dictionary keys/values, or
constructor arguments must be prepared in Block items first.

### 5.4 Index and selector operations

The base and required selector/index expressions are strict prerequisites of
the access. Their sibling positions do not order them. Address, shape, or bounds
validation and the final read/write occur only after prerequisites are ready.
The focused collection design still owns index units, bounds, views, mutation,
and iterator-invalidating behavior.

## 6. Directional transfer

`source -> endpoint` continues to mean only that the value produced on the
left flows to the writable endpoint drawn on the right.

Q03-F represents the operation as:

```text
source value ---------+
                      +--> transfer --> () : unit
endpoint capability --+
```

Source preparation and endpoint preparation are independent prerequisites.
The arrow's graphical direction does not impose source-before-endpoint or
endpoint-before-source preparation. Transfer starts only after both return
normally; successful transfer alone produces Unit.

If both preparations are order-sensitive and no accepted edge orders them, the
direct expression is rejected. The author prepares them in consecutive Block
items in the desired order, then performs the transfer.

Endpoint protocols may add capability, ownership, completion, backpressure,
commit, and cleanup edges without changing the arrow's meaning. Q04 owns copy,
move, borrow, and capture; resource families own their concrete endpoint
protocols.

## 7. Lazy and selecting constructs

Construct-specific control edges are not exceptions to Q03-F; they are the
explicit dependency graph.

### 7.1 Boolean and conditional selection

Short-circuit boolean forms first obtain the decision input and evaluate only
the semantically selected continuation. A conditional evaluates its condition
once and only its selected branch. An unselected branch may be type-checked but
must not execute, complete, acquire resources, or publish effects.

### 7.2 Match

The scrutinee is evaluated exactly once. Arm lexical priority, pattern test,
guard, and selected body form control dependencies. A guard is evaluated only
after its pattern matches; a false guard continues to the next admitted arm;
a completing guard propagates its completion. Pattern legality, overlap,
exhaustiveness, and ownership remain with Q07/Q04.

### 7.3 Settlement

`?|` evaluates exactly one complete operation once. Success bypasses every
recovery expression. A completion selects exactly one admitted named arm or
fallback lazily and at most once. Q03-F connects this frozen local selection to
the surrounding Block stop and publication graph; it does not alter matching,
result join, propagation, or retry policy.

## 8. Resources, pending effects, and concurrency

### 8.1 Pending resource effects

A resource write may remain pending inside a snapshot-backed Block and may be
fused, reordered, or removed before an accepted observation barrier when the
resource topology proves the same logical final state and observations. Block
lexical sequencing orders logical order-sensitive events; it does not require
an immediate physical write for every source item.

Same-resource reads, snapshots, consuming iteration, flush/close, release or
commit hooks, explicit happens-before, and Block exit remain observation
barriers. The optimizer may not cross them without the focused protocol's
proof.

### 8.2 Exclusive access

Unordered exclusive accesses remain illegal unless a resource-topology
happens-before edge orders them. Q03-F does not use source operand position as
a substitute for that edge.

### 8.3 Tasks and streams

Absence of a pure sibling order is not a promise of parallel evaluation.
Task creation, join/settlement, scheduling, fairness, cancellation, stream
demand, backpressure, and cross-stream ordering remain with their focused
designs. Only explicit task/stream constructs may authorize concurrency.

## 9. Completion, divergence, and failure categories

Completion sets describe which nominal completion families may occur; they do
not encode temporal order. If two sibling computations can complete and no
edge orders them, there is no defined “first family”: the program is rejected.

`never` is a proven no-normal-return result and is order-sensitive relative to
effects and completions. A call whose termination/normal-return fact is unknown
is conservatively order-sensitive for conflict checking and may not be
speculated, duplicated, eliminated, or moved across observable edges without a
stronger proof.

Fatal/trap, resource exhaustion, allocation failure, volatile/native behavior,
and other non-Q01 completion outcomes still participate in totality and
optimization proofs. Labeling an expression `pure` must never hide them.

## 10. Optimization rights

The compiler records and verifies separate rights rather than one `pure` flag:

| Right | Meaning | Minimum proof obligation |
|---|---|---|
| `reorder-exact-once` | Move one logical evaluation relative to safe nodes | Same value, completion, normal return, effect/access, resource version, and publication observations |
| `speculate` | Evaluate before it is known to be demanded | Total, completion-free, effect-free, and identity/resource-unobservable |
| `duplicate` | Evaluate more than once physically | Speculation-safe plus identical numerical/identity result and no observable count/cost consequence |
| `elide` | Omit a logically strict computation | Total, completion-free, effect-free, identity/resource-unobservable, and result unused under the exact parent contract |

Common-subexpression elimination, inlining, constant folding, reassociation,
vectorization, fusion, dead-code elimination, and resource-write coalescing
consume these facts. They must preserve:

- strict IEEE floating semantics and approved signed-zero/NaN behavior;
- checked integer overflow and its completion edge;
- exact-once order-sensitive operations;
- lazy/unselected branch non-execution;
- completion-stop, ownership/drop, commit, cleanup, and publication edges.

Constant evaluation follows the same operation summary and dependency graph as
generated execution. It may choose any legal pure topology order but cannot
swallow a completion, force an unselected branch, or publish early.

## 11. Runtime-free lowering contract

The accepted semantics require only compiler-owned static facts:

1. Sema computes canonical operation summaries and access identities.
2. A typed operation DAG records data, control/lazy, completion-stop,
   effect-conflict, explicit happens-before, ownership/drop, commit, and
   backpressure edges.
3. A deterministic scheduler selects a valid topological order and reports
   unordered order-sensitive siblings before lowering.
4. SGIR/CFG preserves branch laziness, exact-once inputs, completion exits, and
   cleanup/publication joins explicitly.
5. Verifier and optimizer consume the same summaries and rights.

No managed exception runtime, ambient error channel, dynamic handler stack,
heap exception object, universal Result wrapper, lazy thunk-update runtime,
resumable continuation, or transparent transaction log is part of Q03-F.

## 12. Required diagnostics

The implementation must distinguish at least:

- two unordered order-sensitive siblings and the missing edge;
- Unknown/native facts preventing safe composition;
- a completion-capable child hidden in an unordered composite/call/index;
- an effect or no-normal-return path moved into an illegal sibling position;
- an optimizer transformation lacking a specific right;
- a lazy/control edge violated by lowering;
- a Block completion that would otherwise publish or run a later item;
- a directional transfer whose two preparations need explicit ordering.

Diagnostics should recommend prebinding/settling in consecutive Block items or
an existing task construct when concurrency is intentional. They must not
recommend an unapproved sequencing operator.

## 13. Explicit non-goals and transferred ownership

Q03-F does not decide:

- value copy/move/borrow/capture, view lifetime, or endpoint ownership (Q04);
- remaining arithmetic relations, conversions, aliases, or NaN comparison
  policy (remaining Q05);
- Unicode/text units (Q06);
- record/pattern legality, partial moves, or exhaustiveness (Q07);
- collection index/slice/iterator protocol (Q08);
- resource-family extension/coherence or concrete scheduling (Q09);
- module visibility (Q10);
- continuation admission, implicit transactions, or new sequencing syntax.

Chain comparison remains rejected until its type, sharing, control, and
completion contract is separately accepted.

## 14. Historical evidence and rejected routes

- [Futhark evaluation](https://futhark.readthedocs.io/en/latest/versus-other-languages.html#evaluation)
  demonstrates eager functional values without a defined operand order or
  automatic parallelism.
- [Haskell 2010 expressions](https://www.haskell.org/onlinereport/haskell2010/haskellch3.html)
  and [I/O sequencing](https://www.haskell.org/onlinereport/haskell2010/haskellch7.html)
  demonstrate the semantic separation between value dependencies and ordered
  actions; Styio does not adopt Haskell's thunk/IO runtime model.
- [OCaml expression semantics](https://ocaml.org/manual/5.3/expr.html) and
  [Scheme R7RS](https://standards.scheme.org/r7rs-html5/index.html) show why
  unspecified operand order becomes a portability trap when ordinary operands
  may contain effects or failures.
- [The Definition of Standard ML](https://smlfamily.github.io/sml97-defn.pdf)
  is the functional counterexample with a left-to-right state/exception
  timeline. It is implementable but was rejected for Styio's dataflow model.
- [Koka](https://koka-lang.github.io/koka/doc/book.html) and
  [Eff](https://www.eff-lang.org/handlers-tutorial.pdf) show that an effect set
  answers “what may happen”, while sequencing still requires computation edges.

Rejected alternatives are: recursive source-order evaluation for every strict
subexpression; implicit left-to-right only after an effect is inferred;
unspecified order for effectful/completing siblings; universal call-by-need;
and implicit rollback of prior external effects.
