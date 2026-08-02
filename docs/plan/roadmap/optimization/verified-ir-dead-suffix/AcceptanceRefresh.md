# OPT-D acceptance refresh

**Purpose:** Bind final OPT-D validation to the synchronized repository-local review evidence.

**Last updated:** 2026-08-02

This receipt binds final validation to the synchronized, once-reviewed OPT-D
contract. It does not change the production design, widen the pass, request a
second review, or replace the final-validation result.

## Reviewed contract bound by this receipt

- The low-level hook is exactly
  `StyioIRPassStatistics run_dead_stmt_elim_pass(StyioIR*)`; there is no legacy
  `void` signature or compatibility route.
- Dead-suffix elimination remains a verifier-guarded, single-walk transform over
  direct `SGBlock`, `SGEntry`, and `SGMainEntry` statement sequences. It returns
  the four deterministic pass counters defined in `StyioIRPassStatistics`.
- Runtime-dead suffix nodes are deleted through stable in-place compaction.
  `SGMainEntry` declarations and bindings required by codegen remain
  compile-time-live, retain order and identity, and have their nested bodies
  traversed.
- Deferred loop-control legality omits OPT-D from the default intermediate
  pipeline and rejects explicit dead-suffix insertion before mutation. Once
  legality is resolved, the strict final-root pipeline may run OPT-D normally.
- General `verify_styio_ir` remains DAG-alias compatible. Pass-manager mutation
  strengthens the verifier boundary with `require_unique_ownership`, rejecting a
  reused pointer or cycle before recording or mutating a pass.

These points are the same bounded repairs accepted by the sole Reviewer in
`Review.md` and synchronized into `Architecture.md` and `Validation.md` by
`OPT-D-RECONCILE`. No later document is authority to redesign those repairs.

## Focused evidence binding

Final validation must use the frozen focused command from the Acceptance Refresh
Node and must obtain the exact ownership-boundary test rather than treating an
empty selection as a pass:

```sh
cmake --build build --target styio_test -j2 && ctest --test-dir build -R '^StyioIRPassManager.RejectsAliasedOrCyclicOwnershipBeforeDeadSuffixMutation$' --output-on-failure --no-tests=error
```

The ownership-boundary test proves both sides of the contract in one scenario:
the general verifier first accepts the reused node as a DAG, then the default
mutating pipeline rejects both that alias and a cycle without mutation or a pass
record. Keeping these assertions in one fixture prevents their shared setup from
being duplicated while retaining a discoverable exact test seam.

## Acceptance handoff

- Sole review outcome: accepted after bounded Reviewer repairs.
- Production redesign or compatibility code after review: none authorized by
  this refresh.
- Second Reviewer: prohibited and not requested.
- Frozen full regression: not run by this refresh and remains the final-validation
  Node's one normal full-regression action.
- Open evidence issue: none.
