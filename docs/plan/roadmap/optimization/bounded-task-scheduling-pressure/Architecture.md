# Bounded Task Scheduling Pressure

**Purpose:** Freeze the OPT-G bounded scheduling architecture for active task syntax.

**Last updated:** 2026-08-02

## Frozen scenario

The active scenario is existing `||>` task launch followed by `?|` task await.
Parser, Sema, lowering, and codegen syntax remain unchanged. This slice replaces
only the ready-queue scheduling beneath the existing task spawn/pull ABI.

Language-level task cancellation is not authorized by the current syntax or
design inventory. Cancellation evidence in this closure means scheduler-owned
close during shutdown: blocked producers and consumers wake, no new task is
accepted, already queued tasks drain, and every accepted task reaches its normal
ready/failure publication. No public cancel function or implicit handle-release
cancellation is introduced.

## One bounded owner

Replace the two queue backends and the scheduler's separate wait mutex/CV with one
`BoundedReadyQueue` owner. It contains:

- one mutex protecting lifecycle and storage;
- one fixed-capacity FIFO deque;
- `not_empty` and `not_full` condition variables using predicate waits;
- an open/closed lifecycle bit;
- scalar counters for accepted pushes, pops, pressure events, producer waits,
  consumer waits, and close wake-ups;
- exact current and peak depth.

The scheduler owns one instance and workers call its blocking pop directly. It
does not maintain another queue condition variable or infer queue readiness from
an approximate size. Capacity is fixed when the singleton starts, defaults to a
bounded repository constant, and may be narrowed by a validated runtime test
configuration before first use. Zero, malformed, and excessive capacities fall
back to the bounded default.

## State and transitions

`push(task)` holds the queue lock. If storage is full, it records one pressure
event for that call, records a producer wait, and waits on `not_full` until a slot
exists or the queue closes. Enqueue publishes one accepted FIFO item and wakes one
consumer. Pressure is a non-failure scheduling fact; it does not set the task's
runtime error state.

`wait_pop()` waits on `not_empty` until an item exists or the queue closes. A pop
removes one FIFO item and wakes one producer. Close is idempotent, changes the
lifecycle once, and wakes all blocked producers and consumers. A push observing
closed returns `Closed`; the scheduler must settle ownership of that unaccepted
task without leaking its context. A consumer returns no item only after closed
and drained.

Task result publication remains the existing release/acquire contract: task work
is written before `ready.store(true, release)`, and pull observes `ready` with
acquire. Queue order never defines language-visible task-result order; awaits and
task handles remain the explicit ordering boundary.

## Complete migration

Remove the legacy unbounded mutex-deque implementation, the busy-spin bounded
implementation, environment-selected backend switching, external queue notify
methods, and the scheduler-owned queue CV. No adapter, old push overload, polling
loop, or parallel scheduler remains. Runtime profile metadata identifies the one
bounded-wait queue and exposes the new pressure/lifecycle counters.

## Representation assessment

A mutex/deque with two predicate CVs is selected because this scheduler already
performs blocking OS-thread work, requires close wake-up, and has a modest bounded
capacity. It provides O(1) amortized push/pop, O(capacity) pointer storage, and a
short correctness proof. A semaphore ring is viable but would require a separate
close protocol; the current so-called MPMC backend is not lock-free; a new
lock-free ring would add reclamation and cancellation complexity without an
observed throughput requirement. Strategy/adapter layers are not retained after
the single backend migration.

## Evidence and bounds

The 256-operation probe uses a small capacity and a concurrent consumer. It must
observe exactly 256 accepted pushes and pops, positive pressure/wait counters,
peak depth no greater than capacity, zero loss/duplication, and a drained queue.
No wall-clock threshold is portable acceptance evidence.

Race evidence is the existing multi-producer/multi-consumer exact-once seam
strengthened with close/wake interleavings. The repository currently supports
ASan/UBSan rather than TSAN; final evidence builds and runs the focused scheduler
test through that established sanitizer route. Adding an unmaintained TSAN mode
is outside this bounded closure.
