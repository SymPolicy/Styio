# OPT-E Bounded Callable Constraint Architecture

**Purpose:** Freeze the repository-local OPT-E callable-constraint worklist design and ownership boundary.

**Last updated:** 2026-08-02

## Scope and frozen outcome

OPT-E replaces both repeated whole-set callable-constraint scans with one
module-local, deterministic worklist implementation in `TypeInfer.cpp`:

- symbolic reduction while one callable SCC is inferred; and
- concrete constraint discharge while one inferred scheme is instantiated.

The worklist is scheduling infrastructure only. It does not add a type-system
layer, a constraint kind, a solver mode, or persistent semantic state. The
existing `CallableConstraintKind` vocabulary remains exactly `Numeric`,
`Comparable`, `Indexable`, `Iterable`, and `Cloneable`.

The following are invariants, not optimization opportunities:

1. `CallableTypeUnifier` remains the only owner of symbolic substitutions and
   retains its occurs check and path-compressing `apply` behavior.
2. The concrete instantiation binding map remains an invocation-local mapping
   from quantified variables to the original `StyioDataType` facts supplied by
   the caller. Normalized values may be used for comparison, but must not
   overwrite richer ownership, state, handle-family, shape, topology, or
   representation facts.
3. `StyioSemaContext` and its name/SymbolId `BindingInfo` maps remain the
   authoritative owners of binding facts. The scheduler stores variable IDs
   and constraint IDs only; it never snapshots or writes `BindingInfo`.
4. Tarjan SCC discovery, sorted dependency traversal, provisional monotypes,
   recursive-group monomorphism, scheme normalization, canonical constraint
   sorting, and deduplication retain their present boundaries and ordering.
5. Numeric-only defaulting remains a concrete-instantiation phase and still
   defaults an otherwise-unbound quantified variable to `i64` only when every
   constraint touching that variable is `Numeric` on its subject.
6. Existing diagnostic text and exception wrapping remain in the existing
   semantic helpers. The scheduler neither catches nor rewrites a
   `StyioTypeError`.

Out of scope are authored generics, new constraint vocabulary, resource
typestate, callable-container support, specialization-cache policy, and any
compatibility or fallback solver.

## Observed recomputation pressure

`reduce_callable_constraints` currently moves all residual constraints through
`pending` and `next` vectors until one complete pass reports no reduction.
`solve_callable_constraint_instance` similarly scans the complete scheme in a
`saturate` loop before and after defaulting. A late binding can therefore make
one earlier constraint ready while every unrelated residual constraint is
copied or revisited. In a reverse dependency chain, the number of attempts is
quadratic even though all facts evolve monotonically.

The useful invalidation event already exists: a symbolic variable is bound, or
a concrete quantified-variable binding strictly gains information. Only a
constraint blocked on that variable can have changed readiness.

## Algorithm comparison

The comparison uses mature solver families as algorithm references, not as new
dependencies or semantic authorities.

| Approach | Current fit | Decision |
| --- | --- | --- |
| Repeated fixed-point scan | It is the simplest control flow and naturally preserves origin order, but copies or revisits every residual constraint after one local fact change. Worst-case attempts are quadratic. | Replace. |
| Dependency-indexed worklist, as used by sparse data-flow and canonical-constraint solvers | Facts are monotone, the constraint set is finite, and readiness changes are keyed by quantified variable. It removes unrelated recomputation while retaining a visible queue and deterministic order. | Adopt in the smallest internal scope. |
| Union-find/unification table, as used by mature Hindley-Milner and compiler inference engines | It can make equivalence-class operations near constant amortized time, but it does not itself remove whole-set constraint scans. Replacing the current term substitution map would also migrate occurs checking, constructor terms, and diagnostic provenance. | Reject for OPT-E; retain the current canonical substitution owner and path compression. |
| Memoization or tabled goal solving, as used by recursive trait/logic solvers | It is valuable for repeated recursive goals and answer reuse. These constraints are finite first-order obligations already partitioned by callable SCC, so tables would need keys, invalidation, cycle semantics, and origin evidence without a demonstrated reuse benefit. | Reject. |
| Additional SCC decomposition of the constraint/variable graph | SCC scheduling is already authoritative at the callable dependency boundary. A second graph would add preprocessing and could blur recursive-group semantics; variable-level invalidation is sufficient. | Reject; retain the existing Tarjan boundary unchanged. |

## Deterministic worklist

### Stable identity and storage

For one run, let `C` be the fixed input constraint count and `V` the variable
ID domain size. Each constraint receives an immutable `ConstraintId` equal to
its position in the input vector. This is its origin for scheduling and
diagnostic priority; applying substitutions must never renumber it.

The scheduler owns only:

- `C` work-item records containing state, origin, and one intrusive waiter
  link;
- `current` and `next` vectors of `ConstraintId`, each reserved once for `C`;
- a `V`-entry waiter-head vector indexed by variable ID; and
- one reusable `V`-entry intrusive binding-delta scratch vector. Its link
  state provides O(1) append and membership without a third `V`-entry bitmap,
  and draining visits only variables changed by the current step.

An item is exactly one of `queued`, `blocked`, or `solved`. A blocked item waits
on the first unresolved variable in its applied subject, using the existing
left-to-right term traversal. Waiting on one variable is sufficient: until
that variable changes the subject cannot become closed, and when it changes
the item is retried and either solves or selects its next unresolved blocker.
Consequently, an item has at most one live waiter edge and there are at most
`C` live edges.

The scheduler performs no heap allocation after these bounded structures have
been initialized. Its reserved storage is at most `3C + 2V` scalar/record
slots, excluding the caller-owned constraints and the existing semantic term
operations. `current.size() + next.size()` and the live waiter count are each
bounded by `C`.

### Wave-preserving execution

The initial `current` frontier is `[0, 1, ..., C-1]`. Items are processed once
in that order:

1. Apply the current symbolic substitutions or concrete bindings.
2. Run the existing semantic reducer/validator.
3. If solved, mark the item terminal.
4. If blocked, index it under its first unresolved subject variable.
5. Record only strict binding changes produced by the step. Drain the waiter
   lists for those variables into `next`, deduplicating by item state.

Woken items are not inserted back into the partially consumed `current`
frontier. At the frontier boundary, sort only the woken IDs by origin, swap
`next` into `current`, and continue. This two-frontier rule is required. An
immediate FIFO append can reorder two earlier constraints according to which
later binder woke them; the two-frontier origin sort reproduces the observable
ordering of the old pass-based reducer without scanning constraints that were
not invalidated.

If an item has not yet been visited in the current frontier, it is not in a
waiter list and simply observes all prior bindings when its original turn
arrives. If it is already queued for the next frontier, another binding does
not enqueue a duplicate.

At quiescence:

- symbolic reduction returns blocked constraints in original origin order,
  after one final application of substitutions; and
- concrete instantiation runs the numeric-default phase once, emits strict
  binding events for newly defaulted variables, resumes the same worklist, and
  then reports the lowest-origin residual as underconstrained.

Scheme-level canonical sorting and deduplication still occur after symbolic
reduction exactly where they do today. Worklist origin controls failure order,
not serialized scheme order.

### Shared semantic seam

There is one internal worklist runner. The symbolic and concrete entry points
remain thin semantic clients of that runner:

- the symbolic step calls the existing constraint reducer, reports
  `solved`/`blocked`, and appends each variable newly bound by
  `CallableTypeUnifier::unify` to the reusable delta;
- the concrete step calls the existing instance validator/matcher, marks a
  ready obligation solved after its one validation, and appends only variables
  whose concrete binding was inserted or strictly refined; and
- the concrete quiescence hook uses one precomputed per-variable default
  eligibility summary built by one traversal of the constraint terms. It must
  not rescan a subject for each variable or rescan every constraint for every
  quantified variable.

This seam may use ordinary module-local functions, records, and callable
parameters. It must not introduce virtual dispatch, a class hierarchy, a
solver service, or runtime algorithm selection. The old `pending`/`next`
whole-vector loop and the `saturate` lambda are removed in the same change;
their function names may remain as call-site entry points, but no fallback or
feature-flagged compatibility path may remain.

## Termination and invalidation proof

The worklist may enqueue an item only initially or after a strict fact change
to the variable on which that item is blocked.

- A symbolic variable changes from unbound to one term once. Later `apply`
  calls only compress that substitution path.
- A concrete binding follows the existing finite monotone progression:
  absent to bound, optionally unspecified numeric width to specific width, or
  normalized/plain representation to the one compatible richer fact.
  Downgrades are ignored and incompatible rich facts throw; no transition
  removes information or oscillates.
- The constraint vector and vocabulary are fixed for the run.

Therefore there are finitely many strict binding events, every event wakes a
finite waiter list, and a validation that creates no strict fact cannot
requeue itself. The run terminates either with all items solved, a finite
origin-ordered residual, or the first existing `StyioTypeError`.

Per-run counters must satisfy:

- `attempt_count == C + requeue_count` after a fully drained run;
- `peak_frontier_count <= C`;
- `peak_blocked_count <= C`;
- `peak_live_waiter_count <= C`; and
- `peak_scheduler_storage_slots <= 3C + 2V`.

The reverse dependency-chain acceptance case in `Validation.md` freezes the
stronger workload bound `attempt_count == 2C - 1`, replacing the prior
`C + (C-1) + ... + 1` scan shape.

## Statistics and lifecycle

`SemaContext.hpp` may expose one read-only
`CallableConstraintSolverStats` value containing at least run count, input
constraint count, attempt count, requeue count, strict binding-event count,
defaulted-variable count, and the three peak counts above plus peak scheduler
storage slots. The common runner returns per-run statistics; the context only
aggregates them for tests and the existing benchmark harness.

Statistics are non-semantic:

- reset them at the start of `prepare_callable_type_schemes`;
- merge symbolic SCC runs and later concrete instantiation runs without
  changing solver decisions;
- expose no mutator to callers; and
- do not include counters in canonical relations, specialization keys,
  dependency digests, diagnostics, or `BindingInfo`.

The worklist, waiter index, default summary, and applied-term scratch state are
request-local and are destroyed after the component or instantiation. Nothing
is memoized in `StyioSemaContext`. Existing clearing of callable schemes,
specializations, semantic body digests, and dependency digests remains
unchanged, so there is no new cross-AST cache invalidation contract.

## Fact and diagnostic preservation

The scheduler must pass the caller's existing terms and binding environment to
the current semantic helpers. In particular:

- it must not reconstruct a concrete subject from its normalized name and
  write that value back to the binding map;
- it must not read or update either `binding_info_` or
  `binding_info_by_sid_`;
- it must retain the current capability-sensitive conflict when two types
  share a normalized representation but differ in ownership, state, shape, or
  topology identity; and
- it must retain the current richer-fact preference when a plain and a
  compatible representation-rich type meet.

The old scan's diagnostic priority is defined as `(frontier, origin)`. The new
runner preserves that priority explicitly. Validators and unification retain
their exact message text; residual underconstraint selection uses the lowest
origin ID. Feature goldens remain the public contract.

## Design pattern assessment

```text
pattern_catalog: refactoring-guru-catalog-22-v1
candidate: none
decision: reject
pressure: Two fixed callable-constraint phases repeatedly scan unrelated residual obligations after one monotone variable binding; the replacement must share scheduling without changing semantic ownership, SCC behavior, defaulting, or first-error origin.
expected_benefit: No GoF pattern adds a verifiable benefit beyond the direct data-structure change. A bounded dependency index and two origin-ordered frontiers provide the measurable benefit: only invalidated constraints are retried, termination is countable, peak scheduler storage is bounded, and diagnostic priority remains stable.
simpler_alternative: One module-local worklist runner, ordinary work-item records, dense waiter heads, two reserved vectors, and direct symbolic/concrete callables are sufficient. Keeping either whole-set loop is simpler in code but fails the recomputation requirement; adding a runtime abstraction is not required.
application: In TypeInfer.cpp only, assign input-position ConstraintIds, process a current frontier, park blocked items by unresolved subject variable, collect strict binding deltas, and process only woken IDs in origin order on the next frontier. Symbolic reduction and concrete instantiation supply direct semantic steps to the same runner. Validation covers reverse-chain attempt counts, adversarial origin ordering, defaulting, SCCs, and fact preservation.
costs_and_rejections: The direct runner adds bounded scheduler state and counters. Strategy is rejected because there is one scheduling algorithm and no runtime-selectable algorithm family; callable parameters are sufficient. Observer is rejected because binding invalidation is synchronous, request-local, and one-to-indexed-many, so subscription lifecycle and hidden control flow add risk. State is rejected because three work-item states are simple data plus conditionals, not behavior-rich state objects. Iterator is rejected because ordinary vector traversal already exposes the required stable order. Union-find, memoization/tabling, and a second SCC graph are rejected for migration cost and lack of benefit against the measured rescan pressure.
```

## Ordered group handoff

1. `OPT-E-DESIGN` freezes this architecture and the executable acceptance
   matrix. No production implementation starts before both documents agree on
   ordering, bounds, defaulting, fact ownership, and invalidation.
2. `OPT-E-IMPLEMENT`, after this Node and its other declared prerequisite are
   complete, removes both whole-set loops, implements the single runner and
   read-only counters, adds the focused internal/feature evidence, and adds one
   bounded callable-constraint sample to the existing benchmark without
   overwriting unrelated benchmark work.
3. The implementation-local Verifier repairs only departures from this frozen
   boundary, then the focused regression runs once.
4. `OPT-E-VALIDATE` receives the changed source, tests, benchmark sample,
   statistics, and focused result. One group Reviewer inspects the complete
   capability, after which the full impacted regression runs once.

There is one implementation Node, so the in-group implementation frontier is
`[OPT-E-IMPLEMENT]`. It has no artificial edge to OPT-D and may execute in
parallel with that independent group once its two declared prerequisites are
satisfied. `OPT-E-VALIDATE` is necessarily serialized after implementation.
