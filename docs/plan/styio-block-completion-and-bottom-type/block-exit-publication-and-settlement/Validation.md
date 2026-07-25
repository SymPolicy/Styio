# Styio Block Exit Publication and Settlement - Validation Matrix

**Purpose:** Map every frozen Block exit requirement to executable, structural, determinism, resource-safety, and documentation evidence.

**Plan:** `styio-block-completion-and-bottom-type/block-exit-publication-and-settlement`

**Last updated:** 2026-07-15

## Requirement mapping

| Requirement | Positive evidence | Negative/structural evidence | Target suites |
|---|---|---|---|
| `REQ-BE-001` | Candidate is evaluated once, moved out of the dying owner set, settled, and only then published. Failed settlement destroys the unobserved candidate after last borrow. | IR verifier rejects early publish, double ownership, dying borrow, candidate rewrite, silent revival, and publish after failed slot. | Sema ownership, StyioIR verifier, feature/pipeline, sanitizer |
| `REQ-BE-002` | Every natural/yield/function/loop/control/failure exit dump contains one verified exit graph and stable schedule. | Relevant cycle fixtures identify source edges; randomized registration/hash order produces identical schedule and diagnostics; no construct-specific epilogue remains. | topology, IR verifier, lowering/codegen, security |
| `REQ-BE-003` | Success exits commit current unpublished state; failure exits abort it; earlier barriers remain visible. | Fixtures reject implicit rollback and prove `break`/`continue` do not bypass frame settlement. | resource topology, stream/frame, feature/pipeline |
| `REQ-BE-004` | Accepted owned children join before borrowed resources drop, and commit/flush/close order follows declared edges. A later accepted frame-retaining feature must add its own proven terminal edge. | Adversarial fixtures fail verification for release-before-join, retained-frame early drop through an admitted feature, double cleanup owner, and cancellation-request-as-completion. | task, resource, admitted-feature integration, sanitizer, stress |
| `REQ-BE-005` | Body failure remains primary; otherwise first semantic ordinal is primary; every later bounded failure is retained and handlers run after the sealed epilogue. | Scheduler completion order cannot change failure order; secondary failure cannot replace primary; half-destroyed-scope handler entry and candidate revival are rejected. | typed-effect, IR/codegen, concurrency stress, diagnostics |
| `REQ-BE-006` | `N == 0` emits no ledger; bounded actions/children use fixed frame-owned segments; independent batches report by preassigned ordinal. | Unbounded hidden obligations, fixed-slot overflow, heap fallback, truncation, global append, and wall-clock primary selection fail closed. | Sema bounds, IR verifier, codegen structural, stress |
| `REQ-BE-007` | Proven total, typed fallible, and fatal actions take distinct verified lowering paths. | Tests prove ignored status/log/abort cannot reclassify a fallible action as total; fatal is not routed as recoverable typed success. | effect classification, native-hook adapters, security |
| `REQ-BE-008` | Parser/token output is unchanged while effect diagnostics and active docs explain the honest external consequences. | Token/grammar diff is empty for this plan; searches find no runtime exception object, dynamic handler search, family ordering authority, legacy epilogue, or first-error-only canonical path. | tokenizer/parser, static audit, docs/convergence, plan gates |

## Required scenario matrix

1. Candidate values: scalar, owned handle, nested container, zero-payload Unit, moved resource, failing candidate discard, and rejected borrow into a dying owner.
2. Exit reasons: natural `}`, `<|`, outer function completion, loop fallthrough, `break`, `continue`, body failure, child failure, accepted external cancellation, and fatal native inability.
3. Action dependencies: independent reverse-registration actions, explicit dependency overriding LIFO, an accepted owned child borrowing a file, pending prepare/merge/commit, flush-before-close, nested inner/outer scopes, and conditional integration coverage for any later admitted frame-retaining feature.
4. Failure combinations: body plus one/many cleanup failures, commit plus close failure, multiple children, expected sibling cancellation, unexpected cancellation, timeout/refusal/join failure, and all-success.
5. Bounds: no fallible actions, one action, maximum declared child capacity, one-past-bound rejection, zero-payload effects, heterogeneous payload slots, and liveness-reused non-overlapping slots.
6. Determinism: randomized AST/resource registration order where semantics are equal, different hash seeds, repeated concurrent completion races, debug/release builds, and supported platforms.

## Structural and complexity evidence

- Instrument graph construction to confirm each semantic node/edge is visited a bounded number of times and schedules are cached per Block.
- Prove graph build O(V + E), stable scheduling O(V log V + E), and codegen linear in the verified schedule plus emitted action bodies.
- Inspect generated IR/native code: no heap allocation, exception object, growable list, reflection, dynamic handler search, or global typed failure append.
- Assert fixed storage size from the IR layout descriptor and verify `N == 0` removes all ledger state.
- Compare schedules and failure ordinals byte-for-byte across randomized runs and concurrency completion orders.

## Migration deletion checks

Search and classify every occurrence of:

```text
set_error_once
cleanup_to_depth
emit_scope_cleanup
pending_resource_names
unordered_map iteration in exit ordering
default return repair
direct return from Block yield
global primary/suppressed failure state
dynamic exception/handler lookup
```

Allowed occurrences are thin adapters, unrelated support logging, or current negative tests. No occurrence may remain an alternate source of exit order, candidate publication, typed failure aggregation, or default result repair.

## Standing gates

Run the configured build and the repository's targeted topology, Sema, IR-verifier, lowering, codegen, task, resource, feature, pipeline, security, stress, and sanitizer tests. Add conditional integration tests only for lexical features already admitted by their own owner decision. Then run:

```powershell
py -3.13 scripts/syntax-convergence-gate.py
py -3.13 scripts/docs-index.py --check
py -3.13 scripts/docs-lifecycle.py validate
py -3.13 scripts/local-info-leak-gate.py --mode worktree
py -3.13 <better-plan-skill>/scripts/manifest_tool.py validate docs/plan
```

`docs-audit.py` is required for final acceptance. Unrelated pre-existing worktree failures are recorded separately and may not be hidden by weakening this plan's checks.

## Final end-to-end acceptance

On one head commit, record exact commands, configuration, result, and artifacts for every `REQ-BE-*` label. Confirm source grammar/token inventories did not grow, every exit path reaches one verified protocol, deterministic schedules and failures survive randomized/concurrent stress, generated code remains runtime-free and allocation-free, all old authorities are removed, and Better Plan state validates.
