# Task-resource capture cost evidence

**Purpose:** Compare correct task snapshot implementations and record evidence-based optimization choices.

**Last updated:** 2026-10-11

**Observed:** 2026-10-11
**Owner:** NODE-COST / TASK-001
**Registered check:** `styio-task-resource-focused`

This is engineering evidence for the existing programme, not candidate
acceptance or Task/Tree delivery. It retains the original NODE-COST measurements as historical evidence. The
reviewer follow-up below supersedes its candidate provenance and cost decision.
Both comparisons use a correct full-snapshot baseline for the measured i64
workloads; they do not establish equivalence for every resource type. No commit, push, pull request,
installation, publication, or full regression was performed.

## Original NODE-COST implementations compared

- **selective (original measured candidate):** `src/StyioCodeGen/CodeGenIO.cpp` `toLLVMIR(SIOTaskCreate*)`
  captures only the bounded rings the task body actually references — the
  state repaired under NODE-REPAIR, byte-identical to that evidence.
- **fullsnap (baseline):** a correct complete-snapshot variant that captures
  every bounded ring visible at task creation. Built by a temporary local
  patch that iterated `bounded_ring_head_slot_` instead of `capture_refs`,
  measured, then reverted; the patch left the worktree byte-identical to the
  repair state (verified by diff hash and file comparison). The saved binary
  is `build/task-resource-cost-20261011/bin/styio-fullsnap`. The measured i64 programs produced identical outputs. No owning-resource
  equivalence claim is inferred from those workloads.

Build configuration: same reused branch and `build/default` macOS arm64 Debug
configuration as BASELINE/REPAIR. No other platform or variant measured.

## Method and workloads

Synthetic amplifiers under `build/task-resource-cost-20261011/src/` (generator:
`gen_and_measure.py`, `measure_loops.py`). R i64 rings `@r0..@r{R-1}` of
capacity `cap`, 64 writes per ring, then T top-level task spawns referencing
the first k rings (`||>` + `?|` + checksum print); loop cells spawn a task
inside a `[0..N] >>` body so runtime capture cost scales with iterations while
exactly one task function is emitted.

Measured per cell and variant:

- compile: `styio build` wall and compiler peak RSS (`/usr/bin/time -l`)
- runtime: native executable wall and peak RSS (median of 3; loop cells get an
  additional 20-rep aggregate average, `STYIO_TASK_THREADS=1`)
- primary path: `styio --file` end-to-end wall per cell
- capture/allocation work: analytic element-copy counts plus exact ctx malloc
  sizes taken from `--llvm-ir` output

Raw programs, logs, IR dumps and full JSON:
`build/task-resource-cost-20261011/` (ignored directory).

## Semantics equality

`run_stdout_sha256` and `jit_stdout_sha256` are identical between variants on
every cell — no output difference on any measured workload. The registered
focused check below ran after that node restored and rebuilt its source. Later
reviewer changes have separate affected-test evidence in HANDOFF.md.

## Results

Cell shape: R rings in scope, capacity `cap`, T task sites, k referenced
rings. `copies` = exact element-value copies emitted per program
(2 passes × captured rings × cap × spawns); `ctx` = exact ctx malloc bytes
per spawn from `--llvm-ir` (cap×8 + 8 per captured ring).

### Compile (`styio build`, single sample per cell; see variance note)

| Cell | selective wall / peak RSS | fullsnap wall / peak RSS |
| --- | --- | --- |
| refs-k0 (R4 cap2048 T16 k0) | 0.21 s / 64 MB | 4.56 s / 501 MB |
| refs-k1 | 6.40 s / 175 MB | 11.37 s / 512 MB |
| refs-k2 | 15.00 s / 299 MB | 19.35 s / 518 MB |
| refs-k4 (k=R) | 31.10 s / 537 MB | 2.94 s / 447 MB |
| refs-k4 recheck | 2.52 s / 447 MB | 2.49 s / 447 MB |
| cap-256 (R4 T8 k1) | 0.74 s / 61 MB | 0.75 s / 99 MB |
| cap-2048 | 3.24 s / 127 MB | 5.52 s / 284 MB |
| cap-8192 | 1.66 s / 236 MB | 5.76 s / 862 MB |
| cap-8192 recheck | 1.19 s / 236 MB | 3.85 s / 862 MB |
| tasks-4 (R4 cap1024 k1) | 1.26 s / 78 MB | 1.32 s / 117 MB |
| tasks-16 | 2.98 s / 114 MB | 6.08 s / 314 MB |
| tasks-64 | 12.79 s / 316 MB | 21.33 s / 944 MB |
| scope-r8 (R8 cap2048 T16 k1) | 6.88 s / 185 MB | 16.17 s / 949 MB |

Wall-time single samples vary with host load (refs-k4 first pass 31.10 s and a
cap-8192 pre-sample of 15.09/21.88 s are recorded as outliers; rechecks are
shown). Peak RSS is an observed process measurement, not a load-independent guarantee.
Only context allocation size and emitted IR work are exact structural evidence;
these grow with captured rings × capacity × task sites in this implementation.

### Primary path (`styio --file`, end-to-end wall)

| Cell | selective | fullsnap |
| --- | --- | --- |
| refs-k0 | 0.09 s | 30.86 s |
| refs-k1 | 17.78 s | 38.67 s |
| refs-k2 | 33.74 s | 50.56 s |
| refs-k4 (k=R) | 66.29 s | 64.36 s |
| cap-256 / cap-2048 / cap-8192 | 0.79 / 8.62 / 32.89 s | 1.75 / 18.45 / 95.97 s |
| tasks-4 / tasks-16 / tasks-64 | 1.87 / 7.87 / 36.50 s | 4.53 / 21.95 / 100.93 s |
| scope-r8 | 15.70 s | 69.00 s |

The JIT path pays IR emission plus the O2 module pipeline over every emitted
copy instruction, so unreferenced snapshots cost the most here — including a
task that references nothing at all (k0: 0.09 s vs 30.86 s).

### Runtime (native exe, STYIO_TASK_THREADS=1)

Per-run wall is ~2–6 ms at all cells, so 20-rep aggregate averages (after a
warm-up run) are reported for the loop cells:

| Cell | selective | fullsnap | extra copies in fullsnap |
| --- | --- | --- | --- |
| loop-k0 (512 spawns, k0) | 3.03 ms | 4.79 ms | 8 388 608 |
| loop-k1 (512 spawns, k1) | 3.41 ms | 5.49 ms | 6 291 456 |
| loop-k2 (512 spawns, k2) | 5.38 ms | 5.81 ms | 4 194 304 |
| loop-cap256 (2048 spawns, k1) | 6.21 ms | 6.70 ms | 3 145 728 |

Runtime deltas are small but directionally consistent: the residual runtime
cost is spawn-side — a larger ctx malloc plus more element copies into it —
roughly 0.4–2 ms per 512–2048 spawns at cap 256–2048. Measured at O2-compiled
native code where elementwise copies vectorize to wide load/store pairs
(observed in disassembly). Peak exe RSS deltas are sub-MB to ~1 MB because a
ctx is freed at task completion; the exact per-live-ctx delta is
(R−k)×(cap×8+8) bytes (e.g., loop-k1: 49 176 B).

### Capture/allocation work (exact)

| Probe | selective | fullsnap |
| --- | --- | --- |
| IR probe ctx malloc arg (R4 cap256 T4 k1) | 2 056 B | 8 224 B |
| loop-k1 ctx malloc arg (R4 cap2048 k1) | 16 392 B | 65 568 B |
| IR probe `--llvm-ir` size | 574 523 B | 1 929 310 B |
| IR probe inbounds array GEPs | 4 200 | 16 488 |
| refs-k0 copies / ctx per spawn | 0 / 0 B (no ctx) | 262 144 / 65 568 B |
| loop-k0 allocations per program | 0 | 512 × 65 568 B ≈ 32 MB traffic |

A task referencing no rings gets no ctx allocation at all under selective;
the baseline still mallocs and fills a 4-ring snapshot per spawn.

## Original decision, narrowed by review

Selective capture reduces exact context size and element-copy work when k<R;
k=R emits the same capture work. Single-sample timings and RSS do not establish
universal improvement or zero regressions. Native output equivalence is limited
to the measured i64 programs. Selective capture is retained for its demonstrated
allocation/work reduction, with the permanent semantic coverage in HANDOFF.md.

The original decision against changing ctx into task-local ring storage remains:
that would alter storage ownership. It does **not** exclude bulk copy between the
two existing independent numeric arrays; the reviewer measured and implemented
that narrower change below. Owning resources keep element ownership operations.
A live-window snapshot remains rejected because unwritten slots have defined
read semantics. No shorter-snapshot contract was introduced.

## Executed registered check

| Command | Outcome | Evidence |
| --- | --- | --- |
| `bash docs/plans/task-resource-delivery/checks.sh focused` | Exit 0: 6 run_styio_case fixtures, 110 passed + 2 platform-skipped `styio_test`, 57 topology, 1 contract, 6 runtime cases — after baseline patch revert and rebuild of the current binary | `build/task-resource-cost-20261011/focused-checks.log` |

## Limitations

- Compile wall times are single samples on a shared host; load-sensitive
  outliers are recorded alongside rechecks. IR size, malloc args and copy counts are exact for the selected input. RSS
  and wall time are observations of that execution, not universal bounds.
- Runtime deltas were resolvable only as aggregates at these scales; larger
  capacities/tasks sites were bounded by compile feasibility, not by
  semantics. Per-unit scaling is stated exactly; extrapolation to bigger
  workloads is inference, not measurement.
- Only the macOS arm64 Debug build was measured; O2 dead-store behavior is
  inferred from emitted/disassembled code, not pass logs.
- No full regression, no Pafio/Vityo E2E, no Task/Tree delivery claim.
  The separate HANDOFF evidence defines the compiler candidate; common E2E and
  final regression remain coordinator preconditions outside this graph.

## Reviewer follow-up on the repaired runtime

Reviewer ownership, scheduling, pending-scope and C1 repairs changed the earlier
candidate. A fresh full-snapshot variant was built from the repaired source,
measured and restored before rebuilding the selective compiler. Saved sources,
variant build/restore logs and binaries stay under
`build/task-resource-handoff-20261011/`. The later ordinary-string ownership
change affects no i64 operation in these cells; the bulk-copy experiment uses a
saved elementwise binary immediately before that final optimization.

Six finite i64 cells vary references, capacity, task sites and visible rings.
Every native and JIT result matched its independently calculated checksum.
No secret, service call or billable provider request is part of these workloads.
The full-snapshot baseline is correct for these measured programs; ownership
semantics for other types are established by permanent tests, not this table.

Cell shape is (R, capacity, task sites, referenced rings). Times are single
samples; MB denotes decimal process peak RSS. Each pair is selective/fullsnap.

| Cell / shape | Compile s | Compile peak MB | JIT s | Native ms | Context B / spawn | Element transfers / program |
| --- | --- | --- | --- | --- | --- | --- |
| refs0 (4, 256, 4, 0) | 2.661 / 2.220 | 49.3 / 73.3 | 0.081 / 0.733 | 413.1 / 397.5 | 0 / 8224 | 0 / 8192 |
| refs1 (4, 256, 4, 1) | 0.250 / 0.372 | 56.1 / 74.2 | 0.353 / 0.961 | 408.1 / 422.7 | 2056 / 8224 | 2048 / 8192 |
| refs4 (4, 256, 4, 4) | 0.755 / 0.175 | 76.9 / 48.0 | 1.859 / 1.772 | 487.3 / 407.5 | 8224 / 8224 | 8192 / 8192 |
| cap16 (4, 16, 4, 1) | 0.260 / 0.228 | 48.8 / 50.7 | 0.078 / 0.105 | 491.6 / 412.7 | 136 / 544 | 128 / 512 |
| tasks16 (4, 256, 16, 1) | 0.760 / 1.469 | 72.5 / 147.1 | 1.735 / 3.969 | 485.6 / 457.4 | 2056 / 8224 | 8192 / 32768 |
| scope8 (8, 256, 4, 1) | 0.355 / 0.663 | 57.9 / 99.6 | 0.465 / 2.006 | 415.0 / 410.7 | 2056 / 16448 | 2048 / 16384 |

Evidence: `cost/results.json`, `measure_cost.py`, `build_cost_variant.py` and
per-cell logs/IR beneath the handoff build directory. Element transfers count
both logical snapshot passes (2 × captured rings × capacity × task sites), not
optimized machine instructions. Context sizes are checked against emitted
malloc operands. The k=R sample demonstrates why wall/RSS are not a universal
no-regression promise. Selective capture is retained for its exact reduction in
allocation and copy work for k<R; no native speedup claim follows from this table.

### Bulk numeric copy experiment and retained change

The operator's narrower proposal was applicable: `CodeGenIO.cpp` now emits one
`llvm.memcpy` for each non-owning array at spawn and one at task materialization.
The snapshot size, cursor, two stores, cleanup and ownership model are unchanged.
String/container arrays retain per-element export/import. The intrinsic is the
closest existing LLVM operation; its non-overlap requirement holds for the
separate allocations ([LLVM LangRef](https://llvm.org/docs/LangRef.html#llvm-memcpy-intrinsic)).

For (R=4, capacity=8192, T=8, k=1), the final candidate was compared with its
immediate correct elementwise predecessor and the repaired full-snapshot
baseline. All three matched the expected native and JIT result. One execution
per variant and path; no additional benchmark sweep was repeated.

| Implementation | Compile s / peak MB | JIT s / peak MB | Native ms / peak MB |
| --- | --- | --- | --- |
| elementwise | 14.753 / 351.1 | 34.628 / 446.2 | 459.1 / 2.61 |
| bulk | 1.173 / 178.2 | 0.481 / 124.2 | 826.9 / 2.11 |
| fullsnap | 22.805 / 961.8 | 96.714 / 1059.0 | 420.4 / 3.36 |

Evidence: `cost/bulk-results.json`, `measure_bulk_copy.py`, saved predecessor
`cost/styio-elementwise`, baseline `cost/styio-fullsnap`, and per-path logs.
The current IR contains 16 bulk-copy calls and a 65,544-byte context per
spawn; emitted IR is 234,577 bytes. The logical amount of snapshot data
has not shrunk; numeric copy-emission work is now O(captured rings × task sites)
instead of O(captured rings × capacity × task sites). Runtime data copying is
still proportional to the complete captured arrays.

**Retain bulk numeric copies:** this same-semantics change has measured compiler
and JIT benefit on the targeted high-capacity input plus bounded IR generation.
The sample does not show a native wall-time improvement, and none is claimed.
`TaskScalarHistoryCopyIRDoesNotExpandWithCapacity` permanently checks i64/f64
capacity-8192 IR growth; the final state/task/LSP and task-codegen selections
passed after this change, including f64 precision, owning captures, failure,
native matrix history and reverse task order. See HANDOFF.md for exact counts.

No ctx-as-ring aliasing, live-window capture, persistent topology cache or other
speculative optimization was retained. Old temporary variants remain ignored
measurement artifacts, not supported implementations or downstream candidates.
