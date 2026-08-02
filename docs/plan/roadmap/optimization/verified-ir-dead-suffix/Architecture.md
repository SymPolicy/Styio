# OPT-D Verified IR Dead-Suffix Elimination Architecture

**Purpose:** Freeze the repository-local OPT-D verified dead-suffix design and ownership boundary.

**Last updated:** 2026-08-02

## Frozen outcome

OPT-D adds one verifier-guarded pass-manager pass that removes only direct statement
suffixes that follow an unconditional local terminator. The transform is a single
top-down walk over the existing owning IR tree. It does not build a CFG, compute
uses or liveness, infer that a conditional construct terminates, or remove an
otherwise reachable statement because it appears pure.

With resolved loop-control legality, the pass is enabled with the existing
optimization family at `opt_level >= 1` and runs before canonicalization and
constant folding. That order prevents later passes from walking statements already
proved unreachable. Deferred intermediate fragments omit the pass, and
`opt_level == 0` remains verify-only.

This design is bounded to the established interfaces in:

- `src/StyioLowering/StyioIROptimizer.hpp`
- `src/StyioLowering/StyioIROptimizer.cpp`
- `src/StyioIR/StyioIRWalker.hpp` (consumed unchanged)

Parser, Sema, code generation, runtime behavior, CFG construction, effect analysis,
def-use analysis, and the IR node hierarchy are explicit non-goals.

## Evidence and mature-compiler comparison

The local IR has three statement-vector owners: `SGBlock::stmts`,
`SGEntry::stmts`, and `SGMainEntry::stmts`. Nested control constructs already expose
their bodies as `SGBlock` children through `StyioIRWalker`. The pass manager already
owns before/after verification, deterministic pass order, timing, diagnostics, and
optional before/after dumps. The former `run_dead_stmt_elim_pass` no-op was the
implementation debt replaced by this closure.

Mature compiler designs establish two boundaries that matter here:

- [MLIR requires a block terminator to be the last operation](https://mlir.llvm.org/docs/LangRef/#blocks).
  StyioIR currently permits a tree-owned sequence to contain a direct terminator
  followed by statements, so OPT-D canonicalizes that local representation.
- [LLVM `removeUnreachableBlocks`](https://www.llvm.org/doxygen/Transforms_2Utils_2Local_8cpp_source.html#l02914)
  and [MLIR `eraseUnreachableBlocks`](https://mlir.llvm.org/doxygen/RegionUtils_8cpp_source.html#l00188)
  traverse explicit CFG successors and retain reachability sets. StyioIR has no CFG
  edge representation at this boundary, so importing that machinery would add state
  without improving this pass's proof.
- [LLVM SimplifyCFG](https://llvm.org/doxygen/classllvm_1_1SimplifyCFGPass.html)
  iterates whole-function CFG rewrites to a fixed point. OPT-D neither rewrites edges
  nor creates new termination facts, so one visit is sufficient.
- [LLVM ADCE](https://llvm.org/doxygen/ADCE_8cpp_source.html#l00096) maintains live
  instruction sets, block state, control dependence, post-dominance, and a worklist.
  Those structures are justified for general DCE, but are specifically outside the
  selected dead-suffix family.

Therefore the smallest direct vector transform has the same safety boundary as the
selected feature and strictly lower implementation, allocation, and invalidation
cost than a worklist or dataflow pass.

## Legality contract

### Input and verifier boundary

The canonical execution path is `StyioIRPassManager`. With the default options it:

1. verifies the complete active root before any transform;
2. verifies again at the existing per-pass precondition boundary;
3. runs dead-suffix elimination;
4. verifies the mutated root before any later pass; and
5. stops and returns diagnostics if either verifier boundary fails.

The general `verify_styio_ir` entry remains DAG-compatible: repeated references to
the same node are accepted by default and visited once. A mutating pass boundary
strengthens that contract with `require_unique_ownership`, so either an aliased node
or a cycle is diagnosed before any pass record or mutation. This separates general
structural validation from the owning-tree precondition required by deletion and
replacement passes.

An invalid or inactive node remains an error even when it is located in a suffix
that would otherwise be removed. OPT-D must never be used to sanitize invalid IR.
There is no rollback after an unexpected post-pass verifier failure: the pipeline
fails closed, stops subsequent passes, and the caller must discard the failed
result. No snapshot or compatibility tree is retained.

When loop-control legality is explicitly deferred for an intermediate fragment, the
default pipeline omits dead-suffix elimination but continues canonicalization and
constant folding. Explicitly inserting dead-suffix elimination with deferred
loop-control legality fails closed before mutation.

The low-level `run_dead_stmt_elim_pass` entry remains available for focused tests
and explicit pass-manager dispatch, but its input contract is a verified active,
uniquely owned IR tree. Its former no-op body and inaccurate promise of general
unused-result DCE are removed in place; no old pass, wrapper, alias, or duplicate
traversal remains.

### Statement owners

The transform edits only these owning vectors:

| Owner | Role |
| --- | --- |
| `SGBlock::stmts` | Function, branch, loop, task, stream, handler, and other nested block bodies |
| `SGEntry::stmts` | Entry sequence |
| `SGMainEntry::stmts` | Root entry sequence, with codegen-consumed declarations and bindings retained after a runtime terminator |

All other child collections are traversed through the established walker but are
not interpreted as statement sequences. In particular, argument, literal,
expression, IO-expression, match-arm-map, and handler collections are not truncated.

### Accepted direct terminators

A statement ends only its immediate owning sequence when its dynamic type is one of:

- `SGReturn`: preserves and traverses the return expression, then ends the sequence;
- `SGBreak`: ends the current loop-body sequence after verifier legality succeeds;
- `SGContinue`: ends the current loop-body sequence after verifier legality succeeds.

The terminator itself and every preceding statement remain in the same order and
retain pointer identity. In `SGBlock` and `SGEntry`, every direct sibling after the
first accepted terminator is destroyed exactly once before its pointer slot is
removed. `SGMainEntry` additionally retains `SGFunc`, `SGExportDecl`,
`SGExternBlock`, `SGFlexBind`, and `SGFinalBind` siblings because codegen consumes
their declarations or type metadata during its predeclaration and capture scan.
Other runtime-dead `SGMainEntry` siblings are removed, and retained compile-time-live
nodes keep stable order and have their nested bodies traversed.

The following are deliberately **not** treated as terminators:

- `SGIf` or `SGMatch`, even when all currently visible arms return;
- `SGBlock`, even when its final child is a terminator;
- loops, including syntactically infinite loops;
- calls, resource operations, task creation, stream operations, `SGWaveDispatch`,
  or any purity/effect classification; and
- a terminator nested inside a child sequence when deciding reachability in its
  parent sequence.

These exclusions prevent path-sensitive inference, non-return analysis, and
control-flow propagation from entering OPT-D. They are acceptance-tested negative
boundaries, not deferred partial implementations of this pass.

## Transform and ownership algorithm

Use one final subclass of the established `StyioIRWalker`, with overrides only for
the three statement owners. Each override calls the same private sequence helper;
the `SGMainEntry` call enables compile-time-live preservation.

```text
visit_sequence(stmts, preserve_main_entry_compile_time_nodes = false):
    statistics.statement_containers_visited += 1
    keep = stmts.size

    for i in [0, stmts.size):
        statistics.statements_examined += 1
        walk(stmts[i])                  // trim reachable nested blocks first
        if stmts[i] is SGReturn, SGBreak, or SGContinue:
            keep = i + 1
            break

    if keep == stmts.size:
        return

    write = keep
    removed = 0
    for i in [keep, stmts.size):
        if preserve_main_entry_compile_time_nodes and stmts[i] is codegen-consumed:
            statistics.statements_examined += 1
            walk(stmts[i])
            stmts[write] = stmts[i]
            write += 1
        else:
            delete stmts[i] exactly once
            removed += 1
    stmts.resize(write)                 // stable compaction; no capacity request
    statistics.statements_removed += removed
    if removed != 0:
        statistics.statement_containers_changed += 1
```

The walk never visits a runtime-dead suffix after its first direct terminator.
It visits only the compile-time-live `SGMainEntry` exceptions described above.
Deleting a suffix root relies on the existing owning-tree destructor contract to
release its subtree; the pass neither recursively deletes the same children nor
calls `shrink_to_fit`. Reachable and compile-time-live nodes are never cloned or
replaced.

For `N` original IR nodes and tree height `H`, the transform is `O(N)` including
destruction of removed subtrees, with `O(H)` traversal stack and `O(1)` explicit
pass state. It performs no pass-owned heap allocation, repeated subtree scan,
hash lookup, worklist iteration, or fixed-point loop. The deterministic work proxy
is `statements_examined + statements_removed`; each direct statement slot contributes
at most once at its owning sequence boundary.

## Stable pass-manager interface

Add the smallest explicit pass surface:

- `PassKind::DeadSuffixElimination`;
- `add_dead_suffix_elimination_pass()`;
- stable record name `styioir-dead-suffix-elimination`;
- `StyioIRPassStatistics`, returned by `run_dead_stmt_elim_pass`; and
- a `statistics` member on `StyioIRPassRecord`.

`StyioIRPassStatistics` contains only zero-initialized `uint64_t` counters:

```text
statement_containers_visited
statements_examined
statements_removed
statement_containers_changed
```

`changed()` is derived as `statements_removed != 0`; it is not separately stored.
Non-OPT-D pass records retain zero counters. Timing remains in `duration_ns` and is
never used as deterministic correctness evidence.

At `opt_level >= 1` with resolved loop-control legality, the default order is:

1. `styioir-dead-suffix-elimination`;
2. `styioir-canonicalization`;
3. `styioir-constant-folding`.

Manual pass-manager insertion continues to preserve the caller's explicit order.
Running dead-suffix elimination twice is valid: the second run reports zero removals
and leaves all vectors and pointers unchanged. With deferred loop-control legality,
the default pipeline contains only canonicalization and constant folding; explicit
dead-suffix insertion is rejected before mutation.

## `design_pattern_assessment`

This assessment covers the group and its only material implementation Node,
`OPT-D-IMPLEMENT`.

```text
pattern_catalog: refactoring-guru-catalog-22-v1
candidate: none
decision: reject
pressure: The pass must traverse a heterogeneous owning IR tree, find the first direct local terminator in each of three statement-vector owners, delete each runtime-dead suffix safely while retaining SGMainEntry declarations consumed by codegen, expose deterministic counters, and remain within the verifier-controlled pass pipeline. There is one selected algorithm and no current runtime variation, object-family creation, configurable handler chain, lifecycle state machine, or CFG/dataflow graph.
expected_benefit: No catalog pattern produces an additional verifiable net benefit. The direct walker override plus one vector helper provides O(N) time, O(H) traversal stack, O(1) explicit state, zero pass-owned heap allocation, centralized ownership mutation, and a focused acceptance seam.
simpler_alternative: Reuse the established StyioIRWalker dispatch and implement one ordinary helper over std::vector<StyioIR*>. This is sufficient and is the selected design; a small dynamic-type conditional expresses the closed terminator set more clearly than a new abstraction.
application: In the smallest scope, one final local walker owns four counters and overrides SGBlock, SGEntry, and SGMainEntry. Each override delegates to the same sequence helper; the helper traverses the reachable prefix, classifies SGReturn/SGBreak/SGContinue, stably compacts compile-time-live SGMainEntry nodes, deletes runtime-dead suffix roots once, shrinks the owner vector, and returns counters to the pass record. Acceptance proves all three owners, nested traversal, exact counters, idempotence, verifier-before rejection, verifier-after success, pointer/order preservation, and no replacement IR allocation.
costs_and_rejections: A new Visitor is rejected because StyioIRWalker already supplies dispatch and adding accept/double-dispatch would modify the node hierarchy. Strategy is rejected because only one algorithm is authorized. Chain of Responsibility is rejected because terminator classification is a closed three-way predicate with no runtime ordering. State is rejected because a single local terminated flag/cutoff is not a lifecycle. Command and Memento are rejected because the transform is neither queued nor reversible; rollback would retain deleted trees. Composite is not introduced because the observed IR ownership tree and walker already exist. CFG worklists, LLVM-style SimplifyCFG iteration, and ADCE liveness sets are algorithmic overreach rather than applicable catalog participants here.
```

## Ordered Node handoff contract

1. **OPT-D-DESIGN** leaves this architecture and `Validation.md` as the immutable
   legality, ownership, interface, complexity, and acceptance contract. It changes
   no production or Better Plan state.
2. **OPT-D-IMPLEMENT** replaces the no-op, wires the pass and counters, updates the
   three declared acceptance paths, and leaves one source implementation with no
   compatibility entry or duplicate traversal. Its focused regression is run once
   only after Worker and Verifier complete.
3. **OPT-D-VALIDATE** receives the complete changed OPT-D boundary, performs the one
   group review, confirms no immediate decision issue remains, and then runs the
   frozen full regression once. A rerun requires concrete failure evidence.

There is one implementation Node, so the widest safe group frontier is exactly
`OPT-D-IMPLEMENT` after `OPT-D-DESIGN`; there is no artificial intra-group edge to
add. Other roadmap optimization groups remain independent and outside this design.

## Risks and non-blocking controls

| Risk | Frozen control |
| --- | --- |
| Invalid IR is hidden by deletion | Whole-root verifier must pass before the transform; an invalid node in the would-be suffix is an explicit negative test. |
| A live effect is removed | Only structural reachability after a direct unconditional terminator matters; effects and purity are never queried. |
| Parent reachability is over-inferred | Conditional, nested-block, loop, and call termination never propagates to the parent. |
| Raw-pointer leak, alias, cycle, or double destruction | Mutating boundaries require unique ownership before deletion; delete each runtime-dead suffix root once, then stably compact and shrink pointer slots. Allocation/destructor and alias/cycle evidence cover the seam. |
| A nested block owner is missed | All `SGBlock` children use the established walker; representative nested-body fixtures and all three owner overrides are asserted. |
| Benchmark noise becomes a correctness gate | Exact counters and node reduction are hard gates; elapsed timing is emitted as evidence, not compared to a machine-specific threshold. |
| The current custom benchmark CLI ignores Google Benchmark-style filter flags | The sample label contains `Dead`, the benchmark executes its invariant checks whenever the current binary runs, and the full command must confirm that its JSON output includes the sample; implementing a new benchmark framework is out of scope. |

## Decision blockers

None. The selected pass has a closed terminator set, explicit owner vectors, an
established verifier boundary, and no product-semantics choice outside the current
IR contract.
