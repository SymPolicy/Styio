# Observable Graph Delivery Follow-Ups

**Purpose:** Record unapproved follow-ups from the Styio/Pafio/Vityo observable-graph delivery so later agents can resume from one rollup instead of chat history. Nothing in this file is authorized work.

**Last updated:** 2026-09-06

**Status:** Unapproved. Do not implement these items from this rollup.

## Delivery That Landed

1. Styio publishes an opt-in static topology snapshot after Sema (`emit.observable_static_snapshot`), then an optional parent-to-child delta and opt-in runtime-events v2 (`emit.runtime_observation`). Observation stays disabled unless a qualified compile-plan asks for it.
2. Pafio forwards those emit flags through compile-plan only. Default plans stay byte-identical.
3. Vityo consumes snapshot, delta, and runtime overlay on one graph surface.
4. S3 budget approval is still missing (`styio-benchmark` `approved-result.json`), so runtime observation must not be default-enabled.

## Unapproved Follow-Ups

| ID | Owner | Severity | Draft | Summary |
|----|-------|----------|-------|---------|
| F1 | Styio | High | PLAN-007 | `styio_lspd` crashes when the IDE cache directory is unwritable (`src/StyioServices/StyioIDE/Index.cpp`). |
| F2 | Vityo | Serious | PLAN-VOG-002 follow-up | Execution adapter expects a `workflow_payload_version` envelope that Pafio never emits, so real Pafio sessions lose receipt and diagnostics. |
| F3 | Vityo | Medium | PLAN-VOG-002 follow-up | Publishing identical snapshot bytes leaves the Observable panel stuck at `refreshing`. |
| F4 | Styio | Low | PLAN-006 reviewer | Disabled-mode runtime summary undercounts controller `emitted`. No Vityo impact while observation stays opt-in. |
| F5 | Styio | Authority | PLAN-006 | Record `styio-benchmark` `approved-result.json` before any default-enablement of runtime observation. |
| F6 | Language | Open | — | Compiler still does not emit rename/move/split/merge lineage from Sema. Wait reasons IO/timer/resource/cancellation/cooperative suspend remain schema-only. Snapshots stay unpublished for anonymous single-file compiles. |

## Explicit Non-Goals For This Rollup

1. Do not implement F1–F4 from this document.
2. Do not treat F5 as a license to change defaults.
3. Do not silently change syntax or the essential language model to close F6.
