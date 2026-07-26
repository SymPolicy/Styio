# Styio Ownership, Capture, and Capability

**Purpose:** Define the accepted Q04-Core value/owner/borrow classification,
capture inference, endpoint transfer modes, ownership post-states, and lexical
drop obligations without adding source ownership syntax.

**Last updated:** 2026-07-25

**Status:** Accepted owner decision `Q04-Core` on 2026-07-25.

## 1. Scope and authority

This document is the sole detailed semantic owner for Q04-Core. It defines how
Sema and HIR prove whether an operation copies a semantic value, consumes an
affine owner, or establishes a lexical borrow/view; how closures and structured
tasks capture; how endpoint protocols select transfer modes; and which scope
owns each drop or close obligation.

The source surface does not gain `copy`, `move`, `borrow`, lifetime, capture
list, or capability-constraint syntax. Existing `:=`, `=`, `->`, closures,
tasks, resource sessions, and resource-specific `<<` retain their accepted
grammar. The `$(deps)` portion of a derived binding is its explicit
frame-dependency list, not a closure capture-mode annotation.

Q04-Core composes, but does not redefine:

- Q01-A `OperationSummary(success_type, completion_set)`;
- Q02-INF definition-site principal inference;
- Q03-F strict evaluation, dependency ordering, completion stop, publication,
  and optimizer rights;
- resource-session handle/anchor, non-escape, owned-child join, and reverse
  Close anchors.

Q04 facts enter Q03-F `EvaluationFacts` and its dependency graph. They never add
fields to, reinterpret, or duplicate `OperationSummary`.

## 2. Semantic classification

### 2.1 Value-semantic types

Copying a value-semantic type produces a value that cannot be distinguished by
observable identity or mutation. Its physical representation may use register
copies, `memcpy`, copy-on-write, shared immutable storage, or complete copy
elimination. Those choices do not change source semantics.

Object size, register count, ABI classification, optimization level, and escape
analysis never decide whether a type is value-semantic. A large immutable value
may remain value-semantic under shared representation.

### 2.2 Affine owners

An owner carries the unique obligation to release an observable identity or
resource. It may be used at most once along a consuming path and is never
implicitly copied, even when its representation is one machine word.

Moving or consuming an owner transfers its release obligation. The source place
becomes unavailable at the accepted ownership-commit edge. Styio inserts no
implicit clone, ARC operation, garbage-collector dependency, or dynamic borrow
check when static evidence is incomplete.

### 2.3 Borrows and views

A borrow/view grants non-owning access to an existing place within a statically
proven region:

- shared read borrows may coexist;
- an exclusive mutation or consume conflicts with every overlapping access;
- every borrow ends before owner drop, consume, or rebinding;
- a view cannot be returned into a longer region, stored in a longer-lived
  aggregate, retained by an endpoint, or captured by a task whose join is not
  statically proven before the owner exits.

Borrow regions are compile-time facts. They do not become runtime reference
counts, hidden handles, or source-visible lifetime parameters.

### 2.4 Product propagation

A product is automatically value-copyable only when all stored members have
value semantics. A product containing an owner is affine as a whole, and a
whole-value move transfers every contained owner obligation. A stored
borrow/view propagates its region and escape restrictions to the product.

Field destruction, partial moves, pattern overlap, and destructuring
exhaustiveness remain owned by Q07. Container element ownership, iterator yield
mode, slice/view invalidation, and mutation remain owned by Q08.

### 2.5 Binding mutability is orthogonal

`:=` makes a name final and `=` makes it rebindable. Neither operator decides
whether the occupant is a value, owner, or borrow:

- a final owner may still be consumed;
- a mutable value still copies with value semantics;
- no binding form grants an implicit clone capability.

## 3. Places, aliases, and ownership state

Sema assigns stable place and region identities before lowering. Each relevant
program point has a fail-closed ownership state such as available value,
available owner, borrowed shared, borrowed exclusive, moved, or unavailable.
`Unknown` is not treated as any safe state.

Every control-flow join must produce compatible ownership post-states. A branch
that consumes a source cannot silently join with a branch that leaves the same
source available unless the enclosing construct has a separately accepted,
unambiguous state rule. Missing or contradictory evidence is a static error.

## 4. Rebinding and consume commit

For rebinding whose RHS does not consume the old occupant:

1. evaluate the RHS completely under Q03-F while the old occupant remains
   installed;
2. if the RHS completes before returning normally, preserve the old binding;
3. on normal RHS completion, atomically install the new occupant and commit any
   move required by the RHS source;
4. after the old occupant's last borrow, run its independent drop/close
   obligation.

If dropping the old owner completes, the new occupant remains installed. The
completion propagates and later ordinary Block items do not begin; Styio neither
rolls back the new value nor exposes an uninitialized slot.

If the RHS itself consumes the old binding, that binding becomes moved and
unavailable at the consume edge. A later completion or selected recovery does
not revive the consumed owner. Q03-F's no-implicit-rollback rule applies to
every consume path.

## 5. Capture inference

### 5.1 Closure captures

Capture mode is uniquely derived from semantic type, actual use, escape class,
and admitted capability:

| Use | Derived mode |
|---|---|
| Immutable value-semantic capture | Semantic snapshot copy |
| Read-only, non-escaping owner access | Shared borrow |
| Mutating, non-escaping owner access | Exclusive borrow |
| Escaping closure that requires an owner | Consume, only when the enclosing protocol permits it |

Capture is not overload search and is never selected from object size. If use,
escape, alias, or capability facts do not yield exactly one valid mode, the
definition fails closed. The compiler does not insert a clone and the author
cannot override the result with a capture annotation.

### 5.2 Q02-INF capture-safe gate

For automatic Q02-INF rank-1 generalization, `capture_safe=true` only when a
callable:

- captures nothing; or
- captures immutable value-semantic snapshots only.

Capturing an owner, borrow/view, resource, task, mutable binding, or `Unknown`
does not satisfy this gate. This only rejects automatic generalization; it does
not make every explicitly contracted closure with such a capture illegal.

### 5.3 Structured task captures

A structured task snapshot-copies value-semantic captures and consumes an owner
only when one unique permitted consume mode is proven. A borrow is permitted
only when a static join edge proves that the task completes before owner
consume/drop and its access does not conflict with another alias.

The accepted owned-child join at resource-session exit is one instance of this
proof. Q04-Core does not admit detached task escape, scheduling fairness,
cancellation policy, or a new concurrency construct.

## 6. Endpoint transfer protocol

Every statically admitted endpoint protocol row declares exactly one input mode:
`copy`, `borrow`, or `consume`. The row also declares source and endpoint
ownership post-states for normal success and every completion exit.

`left -> right` continues to express only left-to-right data direction. The
glyph does not choose a transfer mode, imply a deep clone, or order source
preparation before endpoint preparation. Source value and endpoint capability
remain independent Q03-F prerequisites.

A transfer fails closed when:

- no protocol row applies;
- more than one input mode applies;
- any normal or completion exit lacks a post-state; or
- the required source, alias, region, or capability proof is unavailable.

For `consume`, the source remains available before the ownership-commit edge.
At that edge it becomes moved/unavailable and the release obligation transfers
to the endpoint or protocol. A post-commit completion never revives the source.
For `borrow`, the endpoint must prove it cannot retain the view; the borrow ends
when settlement of that transfer finishes.

Resource-specific `<<` retains only the clone/copy meaning granted by its
existing resource protocol. It does not become a universal value clone
operator.

## 7. Capabilities and lexical exit

The v1 facts for copy, borrow, consume, read, write, clone, close, and related
operations are a closed compiler/prelude/admitted-resource-protocol catalog.
They are not structural duck typing or a user-extensible capability algebra.

Every owner not transferred out contributes exactly one drop/close obligation.
Consume transfers that obligation; it never duplicates it. The exit dependency
graph orders:

1. the last overlapping borrow before consume/drop/rebind;
2. required owned-child joins before releasing captured owners;
3. transfer or rebinding commit before the displaced owner's drop;
4. resource-specific flush and close dependencies already accepted by the
   owning protocol.

Obligations without a real dependency use stable reverse registration order,
never pointer, hash, worker-completion, or backend traversal order. Fallible
drop/close uses the existing operation/completion contract and creates no
ambient exception channel.

## 8. Compiler fact boundary

Sema/HIR owns canonical internal facts including:

```text
OwnershipKind
Place
Region
AliasAccess
EscapeClass
CaptureMode
EndpointMode
OwnershipPostState
CapabilityState
DropObligation
```

These facts contribute borrow-conflict, consume, last-borrow, structured-join,
drop/close, transfer-commit, and rebinding-commit edges to Q03-F
`EvaluationFacts` and its DAG/CFG. Verifiers, lowering, optimization, IDE facts,
and cache fingerprints consume this one authority.

The following invariant remains exact:

```text
OperationSummary = {
    success_type,
    completion_set
}
```

Ownership, alias, capture, capability, escape, and drop information must not be
stored in or inferred from `OperationSummary`.

## 9. Diagnostics and failure closure

An ownership diagnostic identifies the source place, required mode, conflicting
borrow or escape boundary, relevant endpoint/capability row, and the point at
which the source would become unavailable. Missing information rejects the
program before lowering.

There is no compatibility path based on object size, implicit deep copy, ARC,
garbage collection, runtime borrow tables, backend order, or a retained legacy
capture-list parser. Removed ownership heuristics and positive compatibility
fixtures are implementation migration debt, not alternate semantics.

## 10. Deferred owners

Q04-Core deliberately leaves these decisions elsewhere:

- Q07: product-field destruction, partial move, pattern binding, overlap, and
  exhaustiveness;
- Q08: container element ownership, iterator yield mode, slice/view
  invalidation, collection mutation, scheduling, and backpressure;
- Q09: concrete resource capability/typestate rows, user extension coherence,
  and resource-family completion policy;
- Q05/Q06: numeric and text types' final value/view classifications;
- F01/F02: continuations and user-written generic/capability syntax;
- future work: transactions, compensation, automatic consume rollback, a
  universal clone operator, and detached task escape.
