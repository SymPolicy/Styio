# OPT-F Resource Typestate Dataflow Architecture

**Purpose:** Freeze one repository-local resource typestate/dataflow slice before production implementation.

**Last updated:** 2026-08-02

## Frozen slice

OPT-F owns one active-syntax file-handle scenario: a handle acquired before a
`?(cond) => { ... } | { ... }` conditional may be closed in either branch, and
any path-sensitive use after the join (`f.path`, read, write, iterate, or close)
must be rejected whenever at least one reachable branch closed the handle.
Closing in both branches is rejected after the join, closing in one branch is
conservatively rejected as maybe-closed, and keeping both branches open remains
accepted. The slice does not add syntax or change runtime settlement.

## State owner and lattice

`StyioSemaContext` remains the sole owner. The existing SymbolId-keyed consumed
resource fact is interpreted as the two-point may lattice for this slice:

- `Open`: the bound file handle is absent from the may-closed fact set.
- `MayClosed`: the handle is present because at least one reachable path closed
  or consumed it.

`Open ⊔ Open = Open`; every other join is `MayClosed`. Binding existence and
type continue to own uninitialized/materialized facts. EOF is a runtime iterator
condition. Runtime `io`, `closed`, `cleanup`, and other failures remain effects;
they never become typestate values.

The string-keyed set remains only the required fallback for invalid/uninterned
SymbolIds. A valid SymbolId is always authoritative, matching the repository's
current dual-key transition rules. No second typestate map, verifier, or codegen
inference is introduced.

## Transfer and structured join

- successful handle acquire or accepted rebind erases the may-closed fact;
- consuming close/release inserts it;
- file-handle property and I/O consumers reject it through the existing stable
  consumed-resource diagnostic path;
- `CondFlowAST` snapshots the incoming facts, evaluates each branch from that
  same snapshot, captures each outgoing fact set, restores the incoming non-
  resource Sema facts, and installs the union of the reachable branch outputs;
- a missing else branch contributes the unchanged incoming set.

The AST is structured and this slice has no backward resource-state edge, so a
generic worklist, CFG, dominance tree, SCC pass, or union-find is not earned.
One forward visit plus one union per conditional is the complete analysis.

## Complexity and storage

Let `N` be visited AST nodes, `C` conditionals, `R` live resource-handle facts,
and `H` conditional nesting depth. Transfers are expected O(1). Copy/union work
is O(C·R) with the existing hash-set representation and memory is O(H·R); the
implementation must expose deterministic counters for branch snapshots, joins,
fact insertions, and peak simultaneously held fact slots. The frozen 256-join
probe must stay within `2*C` branch snapshots, `C` joins, and `3*H*R + O(R)`
temporary fact slots. No per-use graph scan or repeated whole-program pass is
allowed.

## Complete migration boundary

`StyioSemaContext::typeInfer(CondFlowAST*)` must be the only conditional resource
join. Its current implicit fall-through mutation of `consumed_resource_names_`
and `consumed_resource_names_by_sid_` is removed completely. ResourceTopology,
lowering, codegen cleanup, and runtime error settlement continue to consume Sema-
accepted AST/IR and may not independently reinterpret branch typestate. No old
branch-leak behavior, compatibility flag, or parallel solver remains.

## Pattern assessment

This is monotone forward dataflow over a two-point powerset lattice, following
the standard compiler analysis model. None of the GoF patterns improves it:
State would turn two set-membership facts into heap objects, Observer would
duplicate invalidation, Strategy would invent interchangeable semantics, and
Visitor is already supplied by the AST dispatch. Direct structured transfer and
union is the smallest design.

## Implementation ownership

- `src/StyioSema/SemaContext.hpp`: bounded statistics and resource-fact snapshot
  helpers.
- `src/StyioSema/TypeInfer.cpp`: `CondFlowAST` transfer/join and the existing
  acquire/consume/use seams.
- `tests/typeinfer_internal_test.cpp` and
  `tests/security/styio_security_test.cpp`: exact positive/negative scenarios.
- `benchmark/internal/core_bench.cpp`: one 256-conditional resource-state probe.
