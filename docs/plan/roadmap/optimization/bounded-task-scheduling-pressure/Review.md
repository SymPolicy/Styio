# Bounded Task Scheduling Pressure Review

**Purpose:** Record the unique full-chain OPT-G scheduling review.

**Last updated:** 2026-08-02

## Reviewed boundary

The unique group review traced both frozen requirements through the one active
`||>` launch plus `?|` await route, the queue/scheduler ownership seam, runtime
profile and CLI serialization, focused concurrency tests, and the exact
256-operation benchmark probe. Unrelated parser, Sema, IR, and stream branches
were not audited.

The selected direct mutex/deque design remains justified. The implementation has
one `BoundedReadyQueue` storage/lifecycle mutex and two predicate condition
variables. The scheduler no longer owns a readiness CV or a second queue path.
No strategy, adapter, polling backend, public cancellation surface, or language
semantic expansion remains.

## Confirmed invariants

- A full `push` records pressure and producer-wait facts once for that call,
  blocks on `not_full`, and either accepts exactly one item or returns `Closed`.
- Close is idempotent, wakes all blocked producers and consumers, rejects later
  pushes, and lets accepted FIFO items drain before `wait_pop` returns no item.
- A task rejected only by scheduler shutdown is run to normal ready/failure
  publication, so its context and handle do not become stranded. Handle release
  remains a wait-for-completion operation and does not acquire cancellation
  meaning.
- Task result writes precede `ready.store(true, release)`; pull and release paths
  observe readiness with acquire operations.
- The old mutex-deque backend, busy-spin bounded backend, backend-selection
  environment variable, external queue notification methods, and scheduler
  readiness CV are absent from production paths.
- Capacity is fixed at construction, invalid values fall back to the bounded
  default, runtime metadata identifies `BoundedWait`, and queue pressure,
  wait/depth, accepted/pop, close-wake, and closed fields reach frontend-profile
  JSON.
- The active task-group fixture uses the real parser-through-runtime route with
  a capacity-one queue. Queue tests cover capacity-one blocking, producer and
  consumer close wake-up, accepted-item drain, close interleavings, and MPMC
  exact-once behavior.
- The benchmark performs exactly 256 accepted pushes and pops with positive
  pressure/wait facts, no loss or duplication, bounded peak depth, and a closed,
  drained final state before emitting `bounded_task_pressure_256` JSON.

## Reviewer repair

Updated `docs/teams/PERF-STABILITY-RUNBOOK.md` to remove the retired
`BoundedMPMCReadyQueue`, mutex-deque selector, and old queue-kind description.
The runbook now names the single bounded-wait queue and its current evidence.
Strengthened the real frontend-profiler CLI smoke so it requires queue kind and
every new capacity, depth, pressure/wait, accepted/pop, and lifecycle field.

## Final-validation routing blocker

The frozen final CTest expression currently anchors the suite alternatives as
complete test names. It therefore skips every `StyioBoundedTaskScheduling.*`
and `StyioSecurityBoundedTaskScheduling.*` case while an unrelated exact test
keeps `--no-tests=error` from detecting the omission. It also retains the
removed `BoundedMPMCReadyQueue` suite name.

The final command also has no ASan/UBSan scheduler leg. The established
`scripts/checkpoint-health.sh` sanitizer route cannot stand in for it: that
route builds `styio_test` and selects two `StyioDiagnostics` cases, not
`styio_runtime_scheduler_test` or `StyioBoundedTaskScheduling.*`.

Before final regression advances, the native main must update the frozen final
contract to:

1. match the bounded scheduling and active-syntax cases by their dotted test
   prefixes, retain the exact extern scheduler case, and include the real
   frontend-profiler CLI smoke;
2. parse the emitted benchmark JSON and assert the exact 256 accepted/popped
   values plus positive/equal pressure-wait, bounded peak depth, drained depth,
   and closed state rather than checking only the sample label; and
3. configure the repository ASan/UBSan flags in a separate build directory,
   build `styio_runtime_scheduler_test`, and run
   `^StyioBoundedTaskScheduling\.` there with the established ASan/UBSan runtime
   options and `--no-tests=error`.

This is a regression-routing repair, not a developer decision. The Reviewer did
not run the frozen full regression or sanitizer leg.

## Decision issues

```json
[]
```
