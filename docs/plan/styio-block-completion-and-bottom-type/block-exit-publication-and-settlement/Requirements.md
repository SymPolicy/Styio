# Styio Block Exit Publication and Settlement - Requirements

**Purpose:** Define the source-observable and implementation-safety requirements for frozen decisions `P01.12-A`, `P01.12-B`, and `O05-Q06`.

**Plan:** `styio-block-completion-and-bottom-type/block-exit-publication-and-settlement`

**Last updated:** 2026-07-15

## User problem

Styio currently reaches natural Block exit, `<|`, loops, `break`, `continue`, function completion, runtime failure, and cleanup failure through different partial code paths. Cleanup is split by resource family, pending commits can inherit hash iteration order, and the global support state retains only one error. A candidate can therefore be returned before all obligations are settled, a task can outlive a resource it borrows, or a later cleanup failure can be hidden. Declaring those operations infallible would not remove their physical failure modes.

## Target users and workflows

- Language authors relying on an ordinary Block or function result to mean completed success.
- Resource, stream, task, and future admitted lexical-feature authors defining lifetime and settlement contracts.
- Compiler maintainers adding a new exit action or control-flow edge without duplicating epilogue logic.
- IDE, diagnostic, test, and release maintainers who need deterministic failure identity and source spans.

## Functional requirements

### REQ-BE-001 - Publication follows settlement

A Block result moves exactly once into an immutable epilogue-owned candidate slot. Ordinary `T` becomes observable only after required logical commit and every non-transferable lexical exit obligation reaches its terminal outcome. Failed settlement invalidates and destroys the unobserved candidate after its last permitted borrow; recovery produces an explicit replacement value and never revives the candidate.

### REQ-BE-002 - One verified exit-action dependency graph

Natural `}`, `<|`, outer function completion, `break`, `continue`, typed failure, and cancellation lower to the same typed exit-graph form. The Block is sealed at epilogue entry. Exit actions and edges derive from lexical nesting, ownership, borrow/capture, commit, happens-before, resource-hook dependencies, and candidate ownership. Dependency edges override reverse acquisition/registration; cycles are source-located Sema or IR-verifier errors.

The deterministic ready-node key is deeper lexical scope, then later acquisition/registration, then stable source/node identity. Neither hash iteration nor pointer identity participates. An edge requires the predecessor to reach a terminal outcome, not necessarily success, so an ordinary failure cannot silently suppress must-run cleanup.

### REQ-BE-003 - Exit reasons select honest commit or abort behavior

Natural `}`, `<|`, `break`, and `continue` are successful control exits and commit the still-unpublished current logical frame before publication or branching. Typed failure and cancellation abort only still-unpublished pending state. Earlier publication barriers and irreversible external effects remain committed; arbitrary rollback is never fabricated. Normal exit joins owned children without implicitly cancelling them. A failure exit may request cancellation only where a separately declared capability exists, and still joins every non-transferred child before releasing reachable resources.

### REQ-BE-004 - Lifetime dependencies precede release

The graph represents required order such as `JoinOwnedChild -> DropBorrowedResource`, `Prepare/Merge -> LogicalCommit`, and `pending commit -> flush -> close`. Any separately accepted lexical feature that retains a frame or resource must contribute its proven terminal dependency before release; this plan does not activate continuations or preselect their lifecycle. A cancellation request is neither completion nor join. Resources, owned children, candidates, and nested containers have exactly one cleanup owner; compiler and family hooks cannot double-release them.

### REQ-BE-005 - Bounded typed failures use fixed compiler-owned storage

Each statically known or explicitly bounded fallible action has a stable semantic ordinal and at most one fixed bit/payload slot. An existing body or accepted external cancellation cause remains primary; otherwise the first failed semantic ordinal is primary. Every later failure remains present as secondary evidence. Child segments merge by stable spawn/registration ordinal while preserving each child's local causal order.

The handler runs only after every runnable action finishes and the ledger is sealed. It dispatches through compiler-emitted direct CFG and cannot observe a half-destroyed Block or register a new obligation. All present typed failures must be settled before recovery can produce a replacement value.

### REQ-BE-006 - Unbounded work is explicit

A Block with no fallible exit action emits no failure ledger. A statically registered action or bounded child collection receives fixed storage proven before codegen. An API that can create an unbounded number of independently fallible exit obligations cannot hide them behind ordinary `T`; it declares a finite bound or returns an explicit task, stream, effect, settlement, or resource completion capability. No overflow, truncation, global list, or heap fallback is permitted.

Independent ready actions may execute concurrently only after receiving stable semantic ordinals. Failure reporting uses those ordinals, never wall-clock completion or scheduler order.

### REQ-BE-007 - Failure classification is truthful and internal

Every action is classified internally as:

- `total` only when compiler/resource-contract proof excludes typed recoverable failure under valid invariants;
- `fallible(E)` when it has a statically known nominal completion family and fixed payload slot;
- `fatal` when native execution cannot safely continue and ordinary typed recovery is not promised.

Deleting a channel, ignoring an OS result, logging out of band, or terminating does not prove an action total. Fatal process/native inability remains outside typed aggregation because execution itself cannot be guaranteed to continue.

### REQ-BE-008 - No new source syntax and one implementation

The exit graph, candidate slot, action classes, ordinals, bitsets, and payload slots are compiler/IR details. This plan adds no source token, keyword, constructor, exception type, or authored classification. Real `fallible(E)` effects remain visible through the language effect contract and source diagnostics; decided D02 forbids adding an ordinary value-fallback surface here.

Compiler, resource topology, IR, lowering, codegen, native hooks, tests, and active documentation converge on one implementation. Construct-specific epilogues, family-split ordering authority, default-result repair, and first-error-only canonical state are removed rather than retained as compatibility.

## Non-functional constraints

1. Generated execution state is stack allocated or uses an already preallocated contiguous ledger; no `malloc`, `new`, GC, managed exception object, growable failure list, reflection, or dynamic handler lookup is allowed.
2. Graph construction is O(V + E). A stable Kahn schedule may use O(V + E) graph memory and O(V log V + E) time with a deterministic ready queue; codegen consumes the verified schedule without recomputing topology.
3. Fixed failure storage is O(F + inline payload bytes), where `F` is the statically proven bound. Liveness may reuse non-overlapping slots, and zero-payload effects use only status bits.
4. Normal failure does not stop runnable cleanup. Impossible successors receive explicit causal incomplete status rather than silent success.
5. Diagnostics and result/failure order are deterministic across optimization levels, platforms, hash seeds, and thread schedules.
6. The migration is complete and singular. No legacy mode, dynamic fallback, or alternate cleanup authority remains.

## Scope and non-goals

Owned modules include `StyioResourceTopology`, `StyioSema`, `StyioIR`, `StyioLowering`, `StyioCodeGen`, accepted task/resource hooks in `StyioExtern`, and affected tests and active docs. Conditional integration for a later admitted lexical feature belongs to that feature's plan. Reopening D02, continuation admission, new resource-family APIs, cancellation API design, transactions, compensation syntax, and general runtime exception support are outside this plan.

## Final acceptance target

On one head commit, all `REQ-BE-*` labels have mapped executable or structural evidence. All exit paths enter one verified epilogue; candidate publication cannot precede settlement; lifetime edges are enforced; multiple bounded failures remain deterministic and typed; `N == 0` emits no ledger; generated code performs no dynamic allocation or handler search; and no source grammar/token inventory changes because of this implementation mechanism.
