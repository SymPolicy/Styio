# OPT-D Verified IR Dead-Suffix Elimination Validation

**Purpose:** Define repository-local acceptance evidence for the OPT-D pass family.

**Last updated:** 2026-08-02

## Acceptance rule

OPT-D is accepted only when the pass removes exact direct suffixes, preserves every
reachable statement and value, remains verifier guarded, reports deterministic
statistics, demonstrates linear work and no replacement IR allocation, and emits a
measured benchmark sample. Passing a build, observing a smaller dump, or deleting a
pure literal alone is insufficient.

The normal Better Plan budget is one focused regression after the implementation
Verifier and one full regression after the single group Reviewer. Commands in this
document are not run speculatively by the Designer or Worker.

## Frozen executable seams

### 1. Legality and exact transformation

Add focused cases under a CTest-discoverable suite matching
`StyioSecurityIROptimizer.*`:

| Fixture | Required assertion |
| --- | --- |
| `SGBlock [live-effect, SGReturn(value), dead-effect, dead-value]` | Verifier succeeds before and after; the live effect and return retain identity and order; the return value is unchanged; both direct suffix roots are gone. |
| Loop body `[live-effect, SGBreak, dead-effect]` | The legal break remains; exactly one direct suffix root is removed. |
| Loop body `[live-effect, SGContinue, dead-effect]` | The legal continue remains; exactly one direct suffix root is removed. |
| Terminator in final position | No mutation and zero removals. |
| Empty sequence | No mutation and zero removals. |
| Multiple terminators in one owner | The first direct terminator is retained and every later statement, including later terminators, is removed. |

The dead suffixes deliberately contain effectful nodes. This prevents a false-positive
implementation that merely revives the former literal/purity wording instead of
proving structural unreachability.

### 2. Owner and nested traversal coverage

Add a parameterized or table-driven case that places the same valid dead-suffix
shape directly in each owner:

- `SGBlock::stmts`;
- `SGEntry::stmts`; and
- `SGMainEntry::stmts`.

Add one constructed deep tree whose reachable path includes representative function,
branch, match-arm, loop, task, and stream/IO block parents. Every nested `SGBlock`
on that path is trimmed once. Sibling statement order and retained pointer identity
must be unchanged.

The following negative fixture remains unchanged in its parent sequence:

```text
[SGIf(then = [SGReturn(...)], else = [SGReturn(...)]), live-effect]
```

Both child blocks may be trimmed internally, but `live-effect` remains because OPT-D
does not infer all-arms termination. A second negative fixture uses a direct
`SGBlock` ending in `SGReturn` followed by a parent sibling; the parent sibling also
remains. These cases freeze the no-CFG/no-propagation boundary.

Add an `SGMainEntry` case with a direct runtime terminator followed by runtime-dead
effects interleaved with `SGFunc`, `SGExportDecl`, `SGExternBlock`, `SGFlexBind`, and
`SGFinalBind`. The effects are removed, all five codegen-consumed node families keep
identity and stable order, and nested function bodies are traversed. Real JIT
execution must also prove that a function declared after the terminator remains
available to the predeclaration scan.

### 3. Verifier gates and pass-manager record

Add cases under `StyioIRPassManager.*` that prove:

1. the default `opt_level == 1` record order is exactly dead suffix,
   canonicalization, constant folding;
2. the dead-suffix record name is `styioir-dead-suffix-elimination`;
3. its before/after verifier flags are true on a valid transform;
4. `opt_level == 0` produces no pass records and does not mutate the root;
5. a malformed or inactive node placed after an otherwise accepted terminator is
   rejected by the pre-pass verifier, produces no dead-suffix record, and remains in
   the owner vector; and
6. a verifier failure stops all subsequent pass execution;
7. the general verifier accepts a valid DAG alias, while the mutating pass boundary
   rejects both aliased and cyclic ownership before producing a pass record or
   changing the owner vector; and
8. when loop-control legality is deferred, the default intermediate pipeline omits
   dead-suffix elimination and preserves the fragment, while explicit insertion of
   the pass fails closed before mutation.

IR dumps, when enabled, must show the suffix in the dead-suffix record's `ir_before`
and omit it from that record's `ir_after`. The pipeline `initial_ir` equals the first
record's input, and `final_ir` equals the last successful record's output.

### 4. Deterministic statistics and idempotence

For a fresh nested fixture, assert exact values for:

- `statement_containers_visited`;
- `statements_examined`;
- `statements_removed`; and
- `statement_containers_changed`.

Run the pass on two independently constructed but structurally identical fixtures
and assert identical counters. Do not compare `duration_ns`. Run it a second time on
one already-trimmed fixture and assert:

- zero statements removed;
- zero containers changed;
- identical IR dump;
- identical vector sizes, order, and retained pointer identities; and
- successful verification.

For flat fixtures of increasing size, assert the deterministic linear-work equation:

```text
work_units = statements_examined + statements_removed
```

Every direct statement slot in the owner contributes at most one unit. There is no
fixed-point count, repeated-subtree count, or worklist growth.

### 5. Ownership and allocation

Use the existing session IR allocation statistics around a valid constructed fixture.
Snapshot after construction, run the low-level transform once, then snapshot again.
The focused case must prove:

- no new IR arena or raw-node allocation is attributed to the transform;
- live-node count falls by the exact recursively owned suffix size;
- destructor count rises by that same exact size;
- retained nodes are not destroyed or cloned; and
- destroying the final root releases the remaining tree once.

The test must remain compatible with address/leak sanitizers when those are enabled,
but enabling a new sanitizer configuration is not part of OPT-D.

### 6. Semantic-equivalence fixtures

Create two independently owned valid roots for each selected observable case:

1. a root containing a direct terminator plus unreachable suffix, passed through the
   verified dead-suffix pipeline; and
2. an expected root constructed without that suffix.

Require equal verified retained IR and equal observable result for:

- a function return with a live side effect before the return and an effectful tail;
- break with an effectful tail in a bounded loop; and
- continue with an effectful tail in a bounded loop; and
- a main-entry call whose function declaration appears after a runtime terminator,
  with only the runtime-dead sibling omitted from the expected root.

Use the existing real codegen/JIT or runtime test path for the observable result; do
not introduce a test-only interpreter that could duplicate the optimizer's mistake.
Capture and compare the return value and/or ordered output trace appropriate to each
fixture. Both roots must verify before execution. Structural equality alone does not
replace this differential seam.

### 7. Measured value and benchmark evidence

Add a benchmark sample in `benchmark/internal/core_bench.cpp` with a stable label
containing `Dead`, for example `DeadSuffixFlat4096`. Each timed iteration owns a fresh
valid IR fixture so later iterations cannot benchmark the idempotent no-op state.
The sample must:

- run through the pass manager, not a duplicate benchmark-only algorithm;
- contain a non-empty live prefix, one accepted terminator, and a large suffix;
- fail the benchmark process if verification fails or exact removal counters differ;
- prove the resulting direct statement count and output verifier result;
- confirm no replacement IR nodes are allocated by the transform; and
- emit elapsed timing plus before/after node-count evidence in the benchmark JSON.

Use at least two flat sizes whose deterministic work counters scale linearly. Timing
is recorded for comparison, but no absolute nanosecond limit or machine-specific
speedup ratio is a hard gate. The current custom benchmark binary may ignore the
Google Benchmark-style filter arguments in the frozen full command; therefore the
sample's own invariant checks and presence in emitted JSON are mandatory. The label
must contain `Dead` so a future functional filter continues to select it.

## Focused regression — implementation Node

After the Worker and Verifier have completed, the state owner runs exactly once:

```sh
cmake --build build --target styio_test styio_core_bench -j2 && ctest --test-dir build -R '^(StyioIRPassManager|StyioSecurityIROptimizer)\.' --output-on-failure
```

Required focused outcomes:

- all legality, ownership, negative-boundary, verifier, statistics, idempotence,
  allocation, and differential tests above are discoverable by that regex and pass;
- the benchmark target builds; and
- no focused test relies on suite names excluded by the command.

Do not run the full regression between Worker and Verifier, and do not repeat this
focused command without concrete failure evidence.

## Full regression — final validation Node

After the one group Reviewer and resolution of any immediate decision issue, the
state owner runs exactly once:

```sh
cmake --build build -j2 && ctest --test-dir build -R '^(StyioIRPassManager|StyioSecurityIROptimizer|StyioIRContract)\.' --output-on-failure && ./build/bin/styio_core_bench --benchmark_filter='.*Dead.*' --benchmark_min_time=0.01s && bash scripts/docs-gate.sh
```

Required full outcomes:

1. all selected CTest cases pass;
2. the benchmark exits zero after enforcing its exact removal/allocation invariants;
3. emitted benchmark JSON contains the stable `Dead` sample with timing and node
   reduction evidence;
4. the documentation gate accepts Architecture and Validation; and
5. the Reviewer confirms no general DCE, CFG, parser/Sema/codegen semantics,
   inactive-node workaround, compatibility pass, or duplicate traversal entered the
   change.

A failed command may produce one bounded repair and failure-driven rerun. A quiet or
noisy timing observation without a failed invariant is evidence for later tuning,
not authority to widen OPT-D.

## Handoff evidence

The implementation handoff must identify:

- changed OPT-D source and declared acceptance paths;
- the exact pass record order and counter values from the representative fixture;
- focused regression result from the state owner;
- benchmark sample label and before/after node counts; and
- any verifier diagnostic or ownership issue that remains.

The final-validation handoff must identify the one Reviewer outcome, command
receipts from the repository-local validation run, deferred decisions if any, and
whether the acceptance matrix above is fully evidenced.

## Genuine blockers

Repository inspection at design freeze recorded no blocker. If the existing real execution harness cannot run
the three differential fixtures without changing codegen/runtime semantics, that is
a genuine acceptance blocker and must be raised rather than replaced with a
test-only interpreter or dump-only assertion.
