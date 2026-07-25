# Styio Block Exit Publication and Settlement - Evidence

**Purpose:** Ground the Block exit protocol in frozen owner decisions, current repository behavior, and failure lessons from systems languages and managed runtimes.

**Plan:** `styio-block-completion-and-bottom-type/block-exit-publication-and-settlement`

**Last updated:** 2026-07-15

## Frozen owner evidence

The decision register freezes two connected policies:

- `P01.12-A`: ordinary `T` publishes only after required logical commit and all non-transferable lexical exit obligations complete; later work must be represented by an explicit capability.
- `P01.12-B` / `O05-Q06`: every exit uses one static dependency graph; typed fallible actions use compiler-sized fixed storage; `total`, `fallible(E)`, and `fatal` are internal classifications; no new source syntax or managed runtime is introduced.

Source: [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../../../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md).

## Repository evidence

| Finding | Evidence | Delivery consequence |
|---|---|---|
| RTG records `Flow`, `Ownership`, `Borrow`, `Commit`, and `HappensBefore`, but cycle detection considers only `Flow + HappensBefore`. | `src/StyioResourceTopology/ResourceTopology.hpp:63-72,141-143`; `src/StyioResourceTopology/ResourceTopology.cpp:1521-1541` | Exit dependency extraction must include ownership, borrow, commit, capture, and hook edges and reject all relevant cycles before codegen. |
| Scope drop candidates are enumerated in reverse graph insertion and attached to a destroy sink; lowering validates topology but emits no exit schedule. | `src/StyioResourceTopology/ResourceTopology.cpp:1415-1434`; `src/StyioLowering/AstToStyioIR.cpp:4794` | Reverse order remains a deterministic default only. A dedicated graph-to-IR boundary must make dependencies executable. |
| Natural Block cleanup is split into file, C-string, dynamic/task, and ring passes, generally in forward registration order within a family. | `src/StyioCodeGen/CodeGenG.cpp:3871-3903,3932-3993,4060-4119` | Family passes cannot be ordering authority. A task borrowing a file can otherwise be joined after the file closes. |
| Task release waits for unfinished work, while task contexts can capture scalar, pointer, and handle values. | `src/StyioExtern/ExternLib.cpp:652-663`; `src/StyioCodeGen/CodeGenIO.cpp:511-560` | `JoinTask -> ReleaseBorrowedResource` is a required edge. A cancellation request cannot substitute for join. |
| Accepted lexical features may retain values, children, or resources until their terminal exit obligation. | `docs/design/syntax/BLOCK_COMPLETION.md` §4 | Every feature that is separately accepted contributes its real dependency edges; this plan cannot activate a continuation model or assume a continuation-specific action. |
| Pending commits enumerate names obtained from an `unordered_map`. | `src/StyioCodeGen/CodeGenG.cpp:1745-1757` | Commit visibility and primary-failure identity cannot depend on container iteration order. Stable action ordinals must be assigned first. |
| Natural `}`, `<|`, loop fallthrough, `break`, `continue`, runtime failure, and function return use different partial commit/cleanup paths. | `src/StyioCodeGen/CodeGenG.cpp:2798-2863,3019-3052,3360-3369,3600-3629` | One source path can bypass obligations another performs. All exit edges must lower to one verified protocol. |
| Current support state stores only the first error string. | `src/StyioRuntime/RuntimeState.cpp:61-66`; `src/StyioRuntime/RuntimeState.hpp:9-26` | It cannot be the canonical typed failure bundle and must not grow into a language exception runtime. Frame-owned fixed storage replaces first-error authority. |
| Generated code may not allocate heap state and must use stack or preallocated contiguous ledgers. | `docs/specs/AGENT-SPEC.md:690-706` | Failure aggregation must have a proven bound, no growable list, and no hidden heap fallback. |

## Other-language success and failure lessons

1. Rust [drop scopes](https://doc.rust-lang.org/reference/destructors.html) provide understandable inside-out and reverse-declaration defaults, and moving a value removes it from its old cleanup owner. Reverse drop alone cannot express a task/resource borrow edge, and ordinary `Drop` cannot report typed cleanup failure.
2. Rust [`File`](https://doc.rust-lang.org/std/fs/struct.File.html) and [`BufWriter`](https://doc.rust-lang.org/std/io/struct.BufWriter.html) expose explicit `sync_all` / `flush` because drop-time failures may be ignored. This proves that deleting a channel does not remove physical failure; it merely changes how evidence is lost or escalated.
3. Rust scoped threads guarantee join before scope exit, while the historical guard-based approach was abandoned because leaking the guard could violate lifetime safety. A droppable or forgettable settlement guard is therefore insufficient for Styio-owned children.
4. Zig [`defer` and `errdefer`](https://ziglang.org/documentation/master/#defer) demonstrate native compiler-generated lexical cleanup without a managed runtime. They do not automatically preserve multiple cleanup failures, so static CFG lowering and multi-failure storage remain separate responsibilities.
5. Java ordinary [`finally`](https://docs.oracle.com/javase/specs/jls/se26/html/jls-14.html#jls-14.20.2) can replace an earlier return or failure. Java [`try`-with-resources](https://docs.oracle.com/javase/specs/jls/se26/html/jls-14.html#jls-14.20.3) preserves an original cause plus later close failures, but its `Throwable` and suppressed-list representation is forbidden for Styio. Only the precedence property is retained.
6. C++ destruction during unwinding can call [`std::terminate`](https://eel.is/c++draft/except.terminate) after another destructor failure. This is a valid fatal policy but not a recoverable multi-failure design.
7. Go [`defer`](https://go.dev/ref/spec#Defer_statements) runs before the caller observes a return, but named results can be mutated and cleanup return values can be discarded. Styio instead keeps an immutable candidate and explicit failure slots.
8. Swift structured task groups require children to complete or be cancelled and awaited before scope exit, but completion races are not a stable primary-failure order. Styio preassigns semantic ordinals before any parallel action executes.

## Algorithm and data-structure consequences

- Use dense per-Block `ActionId` values and adjacency storage; actions are static and should not require pointer-keyed maps at codegen time.
- Build edges once from semantic facts, deduplicate them before indegree computation, and cache the verified schedule with the Block IR.
- Use deterministic Kahn topological scheduling. A ready queue ordered by lexical depth, reverse acquisition/registration, and stable source/node identity gives O(V log V + E) scheduling and deterministic batches.
- Preassign semantic ordinals from the verified schedule. Fixed bitsets and inline payload slots allow O(1) failure recording and ordinal scans without allocation.
- Keep child failure segments contiguous and bounded so parent merge is a deterministic span scan, not a dynamic append or global lock.

## Confirmed gaps

1. No executable exit-action graph or relevant-cycle verifier exists.
2. No epilogue-owned candidate representation is shared by every exit path.
3. No typed fixed-layout primary/secondary failure state exists.
4. No direct handler-after-epilogue IR contract exists.
5. Cleanup ordering is split by resource family and construct.
6. Existing accepted task, pending-state, and resource hooks are not unified as typed exit actions; no undecided lexical feature may be assumed merely to populate the graph.
7. Tests do not yet prove deterministic ordering under randomized registration, hash seeds, or concurrent completion.
