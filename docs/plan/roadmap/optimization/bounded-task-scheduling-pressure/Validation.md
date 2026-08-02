# Bounded Task Scheduling Pressure Validation

**Purpose:** Freeze the executable OPT-G scheduling acceptance matrix.

**Last updated:** 2026-08-02

## Requirement matrix

| Requirement | Executable evidence |
| --- | --- |
| `REQ-OPT-G-BOUNDED-QUEUE` | Capacity-one saturation blocks without spinning; pop wakes one producer; close wakes all; exact depth never exceeds capacity; legacy backends and backend-switch environment route are absent. |
| `REQ-OPT-G-CANCEL-PRESSURE-RACE` | Close/drain settlement, pressure/profile counters, MPMC exact-once stress, active `||>` parser-through-runtime fixture, and focused ASan/UBSan execution pass. |

## Focused acceptance

1. Fill a capacity-one queue, block a producer, pop once, and require the producer
   to enqueue without polling; counters show one pressure event and wait.
2. Block producers and consumers, close the queue, and require all threads to
   finish. Accepted items drain exactly once; later pushes return `Closed`.
3. Run multiple producers and consumers with unique IDs and require no loss,
   duplicates, invalid IDs, or capacity violation across close/wake interleavings.
4. Snapshot scheduler telemetry after pressure and require exact lifecycle/queue
   fields while task runtime error remains clear.
5. Compile and run the existing active task group/await syntax in a child process
   configured with a small queue. Require deterministic task results and no leak.
6. Run 256 push/pop operations through a small queue and require exact accepted,
   popped, pressure, wait, peak-depth, and final-depth facts in benchmark JSON.
7. Configure the repository's established ASan/UBSan build and run the focused
   scheduler test. MPMC stress is the race-interleaving proof; sanitizer evidence
   covers memory and undefined behavior on this supported route.

## Regression order

The Worker first builds the queue/runtime/security/benchmark targets and runs only
the focused scheduling tests and exact benchmark oracle. The Verifier may repair
those owned paths and repeats only the smallest affected checks. After one unique
Reviewer, the final node runs the impacted normal build/test matrix, exact probe,
focused ASan/UBSan scheduler leg, diff hygiene, and documentation gate once.

If the sanitizer toolchain is conclusively unavailable on the current platform,
that is a final-validation blocker rather than permission to silently omit the
required sanitizer leg.

## Non-goals

No new cancellation syntax or ABI, task priority, deadline, work stealing,
snapshot join, stream driver queue, pressure observer syntax, timeout policy, or
multi-writer merge is introduced. Shutdown close is internal lifecycle handling,
not a new language cancellation meaning.
