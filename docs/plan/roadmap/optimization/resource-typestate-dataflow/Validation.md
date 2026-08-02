# OPT-F Resource Typestate Dataflow Validation

**Purpose:** Define repository-local acceptance evidence for the selected OPT-F slice.

**Last updated:** 2026-08-02

## Acceptance matrix

1. Acquire `f`, close it in the then branch only, and use `f.path` after the
   join: Sema rejects the use through the existing consumed-resource diagnostic.
2. Close `f` in both branches: the same deterministic diagnostic is emitted.
3. Keep `f` open in both branches: property and I/O use remain accepted.
4. A conditional without an else treats the absent branch as the incoming open
   state, so a close in the then branch still yields maybe-closed.
5. Nested conditionals restore each branch from the same incoming facts and do
   not leak then-branch mutations into else inference.
6. Acquire/rebind after a join reopens the handle through the existing erase
   transfer; a subsequent use is accepted.
7. Runtime failure handlers and error subcodes are unchanged and are not visible
   in the typestate counters.

## Frozen evidence

- Internal tests assert exact SymbolId authority, invalid-SymbolId fallback,
  branch isolation, union results, deterministic counters, and the absence of a
  second state owner.
- Security tests exercise active parser-through-Sema file acquire, conditional
  close, post-join property/I/O use, negative diagnostics, and open/open success.
- The benchmark builds 256 conditional joins over bounded live handles and
  asserts branch-snapshot, join, insertion, and peak-slot budgets in one run.
- Migration inspection must find no conditional path that directly leaves the
  then branch's consumed-resource sets installed, and no compatibility flag,
  CFG solver, or duplicate resource-state map.

## Focused commands

```sh
cmake --build build --target styio_typeinfer_internal_test styio_security_test styio_core_bench -j2
ctest --test-dir build -R '^(StyioResourceTypestate|StyioSecurityResourceTypestate)\.' --output-on-failure --no-tests=error
STYIO_BENCH_WARMUP=1 STYIO_BENCH_MEASURED=1 ./build/bin/styio_core_bench --stdout > /tmp/styio-opt-f-focused.json
rg -q '"label"[[:space:]]*:[[:space:]]*"resource_typestate_conditional_join_256"' /tmp/styio-opt-f-focused.json
```

## Full group gate

The one final group run builds the impacted targets, runs the focused typestate
tests plus existing resource-topology and directly impacted resource security
labels, checks the benchmark oracle, runs `git diff --check`, and runs the
repository documentation gate. A non-empty test selection is mandatory.

## Hard failures

- relaxing post-join use to runtime-only checking;
- treating runtime failure/effect subcodes as typestate;
- name-key state winning over a valid SymbolId;
- a second verifier, CFG, resource-state map, or compatibility branch;
- counters exceeding the frozen budgets or changing existing diagnostics.

## Decisions

No immediate or deferred developer choice is required for this bounded slice.
