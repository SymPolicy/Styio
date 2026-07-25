# Styio Block Exit Publication and Settlement Plan

**Purpose:** Deliver the frozen `P01.12-A..B` / `O05-Q06` Block publication, exit ordering, and bounded multi-failure contract through compiler-generated native control flow.

**Last updated:** 2026-07-15

**Status:** Pending implementation. The owner accepted the logical publication barrier, one static exit-action dependency graph, and fixed compiler-sized failure storage.

## 前置条件

1. The parent [Styio Block Completion and Bottom Type Plan](../../Styio-Block-Completion-and-Bottom-Type-Plan.md) owns lexical Block results, `unit`, `never`, and current-Block `<|` targeting. This child consumes those facts and does not redefine them.
2. The frozen decision is recorded in [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../../../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md), especially `P01.12-A`, `P01.12-B`, and `Q06.detail-A..K`.
3. Generated execution state remains stack allocated or stored in an already preallocated contiguous ledger. The implementation may not introduce `malloc`, `new`, GC, managed exception objects, growable suppressed-failure lists, reflection, or dynamic handler search.
4. Read-only evidence, test discovery, downstream contract confirmation, and documentation audits may run in parallel. Source-writing nodes follow `Checkpoints.json` because the topology, IR, lowering, codegen, resource, and task paths share interfaces and must converge once.
5. Sub-agents may be started for read-only audits and file-disjoint test or documentation work. One coordinator owns semantic decisions, shared interfaces, manifest state, and the final SSOT merge.
6. Shared workflow gates, reusable test-harness entrypoints, or compiler service-contract substrate must land through the `styio-common-foundation` plan first. This child owns only the feature-specific Block exit protocol and must not create a second common foundation.

## Source-surface boundary

This plan adds no source token, keyword, constructor, exception object, or authored `total` / `fallible(E)` / `fatal` annotation. `ExitActionGraph`, `ExitValueSlot`, `ExitFailureLedger`, action classes, semantic ordinals, and fixed slots are compiler and IR concepts.

Implementation-only does not mean semantically hidden. A physically fallible operation cannot become total by deleting its nominal completion family. That family remains visible in the enclosing operation/callable's static completion set and source-located diagnostics. D02 excludes ordinary value fallback; this child plan adds no alternative payload-binding or handling syntax.

## Delivery target

Every natural `}`, `<|`, outer function completion, `break`, `continue`, typed failure, and cancellation edge enters one verified Block epilogue form:

1. evaluate and stabilize the candidate exactly once;
2. seal the Block and derive all exit obligations;
3. execute a deterministic dependency-respecting exit schedule;
4. commit or abort only the current unpublished logical state according to exit reason;
5. retain every bounded typed failure in fixed frame-owned storage;
6. run effect handling only after all runnable obligations reach terminal outcomes;
7. publish the ordinary result, branch, propagate the sealed effect state, or terminate for a truly fatal native condition.

No ordinary `T` is observable while a non-transferable lexical obligation can still fail. Work may remain only behind an explicit task, effect, settlement, or resource completion capability.

## Scope

- Exit dependency extraction and cycle checking in `StyioResourceTopology`.
- Typed exit-action and failure-layout contracts in StyioIR and its verifier.
- Candidate ownership, commit/abort selection, fixed failure layout, and direct handler branches in lowering and codegen.
- Accepted owned-child/task join, pending-state commit, resource flush/release/close, candidate discard, and existing resource-family hooks as exit actions. Later admitted lexical features integrate conditionally and are not activated by this plan.
- Complete migration of natural Block, explicit yield, loop control, function, and typed-failure exits to one protocol.
- Deterministic concurrency batches, bounded child failure segments, diagnostics, tests, active design documents, runbooks, and validation gates.

## Non-goals

- No new completion-binding syntax, `Result` constructor, throw/catch syntax, or authored cleanup annotation.
- No managed language runtime, heap fallback, stack unwinding, dynamic exception dispatch, or global mutable failure bundle.
- No new cancellation API, transaction DSL, resource completion level, or implicit detach rule. Existing or separately accepted contracts are consumed as declared.
- No promise to roll back irreversible external I/O. Only an explicit transaction or compensation contract may make that promise.
- No compatibility epilogue, family-specific fallback path, or positive test for retired first-error/default-return behavior.

## Execution graph

The machine-validated graph is [Checkpoints.json](./Checkpoints.json). Delivery contracts are:

- [Requirements.md](./Requirements.md)
- [Evidence.md](./Evidence.md)
- [Validation.md](./Validation.md)
- [Architecture.md](./Architecture.md)

## 验收条件

All `REQ-BE-*` requirements have source, IR, native-code, concurrency, failure, and structural evidence on one head commit. Every exit edge uses the one verified protocol; deterministic ordering never depends on hash iteration, pointer identity, completion races, or thread timing; no heap/runtime exception mechanism exists; every real typed failure remains observable; and all old construct-specific or first-error-only canonical paths are removed.
