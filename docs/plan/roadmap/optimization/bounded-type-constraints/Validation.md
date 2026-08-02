# OPT-E Validation

**Purpose:** Define repository-local acceptance evidence for the OPT-E callable-constraint worklist.

**Last updated:** 2026-08-02

## Acceptance contract

OPT-E is accepted only when the repeated symbolic and concrete whole-set scans
are gone, both phases use the same dependency-indexed worklist, and all
observable type-system behavior remains equivalent. A timing improvement alone
is not acceptance evidence.

Let `C` be one run's input constraint count and `V` its variable ID domain.
The frozen structural budgets are:

```text
attempt_count == C + requeue_count
peak_frontier_count <= C
peak_blocked_count <= C
peak_live_waiter_count <= C
peak_scheduler_storage_slots <= 3*C + 2*V
```

The scheduler must allocate its bounded work records, two frontiers, waiter
heads, and binding-delta scratch once per run. It may not allocate or copy a
whole residual-constraint vector per frontier. Existing term application and
diagnostic string construction are outside the scheduler-slot count and must
not be cached across runs.

## Focused executable matrix

| Seam | Required evidence | Failure condition |
| --- | --- | --- |
| Single scheduler | An internal test exercises symbolic reduction and concrete instance discharge through the common runner. A review-only search confirms the old `pending`/`next` scan body and `saturate` lambda are absent. | Either old whole-set loop, a fallback solver, or two independent scheduling loops remains. |
| Termination and recomputation | Build a direct internal reverse chain of `C = 256` iterable obligations. Constraints are stored in reverse dependency order; the first subject is seeded with a 256-level list term so each solved obligation exposes the next. Assert no residual, `attempt_count == 511`, `requeue_count == 255`, and every structural budget above. | A timeout/guard is the only termination proof, any unrelated constraint is retried, or counts exceed the frozen values. |
| Quiescent residuals | Run 256 unrelated blocked obligations with no binding event. Assert exactly 256 attempts, zero requeues, one live waiter per residual at most, and residual IDs returned in original order. | Any second scan occurs or residual order changes. |
| Origin diagnostics | Use four origin-ordered obligations: earlier `numeric(x)`, later `numeric(y)`, then one obligation binding `y` to `string`, then one binding `x` to `bool`. Although `y` is woken first, the next frontier must test lower-origin `x` first and preserve the existing `bool` numeric error text. | Immediate requeue reports the later-origin `string` failure first, or the message is rewritten. |
| Strict invalidation | Exercise one binder waking a fan-out of blocked obligations and one obligation receiving multiple compatible facts before its next turn. Assert a blocked ID is queued once, binding-delta membership uses the existing `V` scratch slots rather than a linear duplicate search or third bitmap, validation without a binding change never self-requeues, and wake candidates are origin-sorted. | Duplicate queue entries, polling, per-event linear duplicate scans, extra scheduler storage, or non-monotone binding transitions appear. |
| Numeric defaulting | Cover an unbound numeric-only quantified variable, a transitive numeric constraint, and a mixed/non-numeric constraint. Assert the first defaults once to `i64`, its dependents are woken without a whole-set scan, and the latter remains underconstrained with the existing message. | Defaulting broadens beyond numeric-only, runs more than once, changes public text, or skips a dependent. |
| SCC behavior | Retain the recursive-group and mutual-recursive feature fixtures. Add internal assertions that component membership, `recursive_group`, quantified variables, and canonical relations are identical before and after scheduling for representative single, self-recursive, and mutual-recursive definitions. | Constraint scheduling changes Tarjan traversal, generalizes a recursive call independently, or changes canonical scheme order. |
| Constraint vocabulary | Compile all numeric, comparable, and indexable positive/negative fixtures; exercise iterable and cloneable internally. The enum and serialized names remain the five existing values only. | A new kind, alias, compatibility vocabulary, or changed canonical spelling appears. |
| Binding and representation facts | Seed both name- and SymbolId-keyed `BindingInfo` for list/dict/matrix/task and file/stream representatives, run scheme preparation and concrete discharge, and compare every `final_slot`, `dynamic_slot`, `resource_value`, `value_kind`, and `declared_type` fact afterward. Also retain richer compatible concrete bindings and the capability-sensitive conflict for normalized-equal but ownership/state/shape/topology-distinct values. | The scheduler reads/writes `BindingInfo`, replaces a rich binding with a normalized reconstruction, erases a handle, or accepts incompatible facts. |
| Lifecycle and cache isolation | Run two different ASTs through the same context lifecycle and two concrete instantiations with different bindings. Assert worklist/default metadata is rebuilt per run, statistics reset at scheme preparation then aggregate deliberately, and specialization/dependency digests contain no counter or queue state. | A prior run changes the next result, a stale waiter fires, or statistics affect a key/diagnostic. |
| Allocation and realistic workload | Add one `phase = constraint` sample in `core_bench.cpp` that type-checks a deterministic chain/fan-out of at least 256 inferred callable constraints. Require `peak_frontier_count`, `peak_blocked_count`, and `peak_live_waiter_count` to equal 256 for that workload, fail internally if any structural budget is exceeded, and report peak scheduler slots through the documented allocation-count field. | The sample is absent, does not prove one 256-constraint run, silently catches a budget violation, depends on wall-clock thresholds, or measures only unrelated parser/type-check work. |

The reverse-chain and removal checks belong in the focused implementation
closure. Do not promote checks for exact removed source text into a permanent
repository-wide gate.

## Public behavior matrix

The following existing fixtures are mandatory and their current stdout/stderr
goldens are unchanged:

- `tests/features/callable_constraints`: numeric and comparable instances,
  list/dict indexable instances, transitive numeric propagation, and all three
  mismatch diagnostics;
- `tests/features/inferred_generics`: multi-instance identity, contextual empty
  lists, recursive and mutual-recursive SCCs, generic composition, scalar and
  function expected types, underconstraint, and polymorphic-recursion
  rejection; and
- `tests/typeinfer_internal_test.cpp`: direct worklist counts, ordering,
  defaulting, richer-fact preservation, `BindingInfo` handle facts, and cache
  lifecycle.

Negative fixtures must assert the existing diagnostic substring or complete
golden, not merely that some exception was thrown. Positive fixtures must
assert their existing output and the relevant internal solver counters; a
test that bypasses the worklist is a false positive.

## Focused implementation regression

Run once after the Worker and Verifier have completed the implementation:

```bash
cmake --build build --target styio_test styio_core_bench -j2
ctest --test-dir build -L 'callable_constraints|inferred_generics' --output-on-failure --no-tests=error
ctest --test-dir build -R '^StyioSema' --output-on-failure --no-tests=error
```

Run the bounded benchmark smoke once and require the OPT-E sample to be
present. `styio_core_bench` is the repository's custom benchmark binary; a
Google Benchmark `--benchmark_filter` argument does not select its samples, so
sample presence and the sample's internal budget assertions are the evidence:

```bash
OPT_E_BENCH_JSON="$(mktemp /tmp/styio-opt-e-bench.XXXXXX.json)"
STYIO_BENCH_WARMUP=1 STYIO_BENCH_MEASURED=1 ./build/bin/styio_core_bench --stdout > "$OPT_E_BENCH_JSON"
rg -q '"phase"[[:space:]]*:[[:space:]]*"constraint"' "$OPT_E_BENCH_JSON"
rg -q '"label"[[:space:]]*:[[:space:]]*"callable_worklist_' "$OPT_E_BENCH_JSON"
```

Perform this review-only, one-time migration audit; do not add it to a
long-lived test or gate:

```bash
! rg -n -F 'std::vector<CallableTypeConstraint> pending = std::move(constraints)' src/StyioSema/TypeInfer.cpp
! rg -n -F 'auto saturate = [&]()' src/StyioSema/TypeInfer.cpp
rg -n 'run_callable_constraint_worklist' src/StyioSema/TypeInfer.cpp
```

## Final group regression

After all implementation Nodes are complete, run one Reviewer over the OPT-E
changed and directly impacted paths. Resolve any immediate decision issue,
then run this full impacted regression exactly once:

```bash
cmake --build build -j2
ctest --test-dir build -L 'callable_constraints|inferred_generics' --output-on-failure --no-tests=error
ctest --test-dir build -R '^StyioSema' --output-on-failure --no-tests=error
OPT_E_BENCH_JSON="$(mktemp /tmp/styio-opt-e-final.XXXXXX.json)"
STYIO_BENCH_WARMUP=1 STYIO_BENCH_MEASURED=1 ./build/bin/styio_core_bench --stdout > "$OPT_E_BENCH_JSON"
rg -q '"phase"[[:space:]]*:[[:space:]]*"constraint"' "$OPT_E_BENCH_JSON"
rg -q '"label"[[:space:]]*:[[:space:]]*"callable_worklist_' "$OPT_E_BENCH_JSON"
bash scripts/docs-gate.sh
```

Do not repeat the full regression without new failure evidence. A failure is
repaired in the smallest affected path and only the failed evidence is rerun
before the failure-driven full rerun.

## Evidence handoff

`OPT-E-IMPLEMENT` hands `OPT-E-VALIDATE` all of the following:

1. the common runner and proof that both old scan bodies were removed;
2. per-run and aggregate statistics from the reverse-chain, quiescent,
   origin-order, defaulting, and realistic benchmark cases;
3. unchanged feature goldens and canonical SCC relation assertions;
4. before/after `BindingInfo` and representation-fact comparisons; and
5. the single focused regression and benchmark-smoke results.

Missing counters, an unasserted benchmark sample, a generic "tests passed"
statement, or a second compatibility path is insufficient evidence.
