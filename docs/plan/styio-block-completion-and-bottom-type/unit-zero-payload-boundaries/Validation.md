# Styio Unit Zero-Payload Boundaries — Validation

**Purpose:** Map every frozen Unit-boundary requirement to executable behavioral, structural, performance, and documentation evidence.

**Plan:** `styio-block-completion-and-bottom-type/unit-zero-payload-boundaries`

**Last updated:** 2026-07-14

## Validation matrix

| Requirement | Positive evidence | Negative/structural evidence | Target suites |
|---|---|---|---|
| `REQ-UZ-001` | Unit has a stable canonical `TypeId`; `unit` works as accepted built-in type argument. | Unit differs from Undefined/Never/i64; no lexer keyword/token is added. | TypeTable, parser/typeinfer internal, security |
| `REQ-UZ-002` | Zero-payload values retain all logical states at empty, one, many, and boundary counts. | Static/source audit rejects address/byte-size-derived counts, fake payloads, zero-size division, and unchecked overflow. | runtime unit tests, sanitizer, structural audit |
| `REQ-UZ-003` | `? | unit` distinguishes absent from present `()` and composes with optional normalization. | Missing tag, payload-derived presence, implicit default, and unwrap routes fail. | language feature, typeinfer, StyioIR verifier |
| `REQ-UZ-004` | Unit list operations/iteration and Unit dictionary order/membership/lookup/values work at 0, 1, large n, clone, mutation, and slice boundaries. | Bounds/overflow fail deterministically; no per-element/mapped Unit payload exists. | `externlib_internal_test`, security, feature/pipeline |
| `REQ-UZ-005` | Unit task runs, succeeds, fails, publishes state, and settles once as `()`. | Unit never enters I64 helpers/slots; failure never supplies Unit/absence; double pull remains rejected. | typeinfer/lowering/codegen/runtime task tests |
| `REQ-UZ-006` | Returning C `void` adapts to Unit; nullable and no-return signatures adapt distinctly. | C `void` never maps to Undefined; null cannot inhabit ordinary `T`; Unit is not exported as a C object. | native parser/codegen/security/ABI tests |
| `REQ-UZ-007` | Typed Unit survives Sema and StyioIR while LLVM legally erases payload. | Verifier rejects Unit-to-i64 repair, missing optional tag, `never` value, and invalid ABI mapping. | lowering/codegen/IR verifier/pipeline goldens |
| `REQ-UZ-008` | Docs, tooling, fixtures, compiler, and runtime agree on one contract. | Search proves obsolete branches/assertions/compatibility routes are absent. | syntax convergence, docs gates, repository audit |

## Required stress cases

1. `? | unit`: absent versus present `()`, branch selection, equality/formatting if supported, and no default construction.
2. `list[unit]`: empty, singleton, one million logical elements, push/pop/insert/remove/get/set/slice/clone, full and partial iteration, and overflow/bounds errors.
3. `dict[K,unit]`: insert duplicate/update, remove/reinsert, order, membership, missing/present lookup, clone, and values iteration count.
4. `task[unit]`: not-ready, running, success, typed failure, ready fast path, blocking pull, consumed/double pull, release before/after settlement, and concurrency publication.
5. Native calls: returning `void`, ordinary pointer/value, explicitly nullable pointer, and explicit no-return; source and LLVM signatures must remain distinct.
6. Cross-layer: Sema/IR dumps show Unit identity; LLVM has no fabricated `i64 0` Unit payload; optimizer preserves optional/container/task state.

## Complexity and memory evidence

- Unit list `len`, indexed bounds checks, and mutations remain O(1); iteration is O(n) events and terminates exactly at logical `n`.
- Unit list payload storage stays O(1) with respect to `n`; test instrumentation or a representation-level assertion proves there is no per-element payload allocation.
- Unit dictionary storage is O(number of keys), with no mapped Unit object per key; lookup retains the current expected average complexity.
- Checked logical counters reject overflow before mutation. Sanitizer runs cover empty/large iteration and mutation paths.

## Structural deletion checks

Search and review must classify every occurrence of:

```text
result_name == "unit"
result_type.name == "unit"
task[unit]
CTypeKind::Void
StyioDataTypeOption::Undefined
sizeof(unit)
payload_size
pointer difference in collection iterators
```

Allowed occurrences implement or test the current typed boundary. No occurrence may rewrite Unit to an integer, infer logical state from zero bytes, or map returning C `void` to compiler Undefined.

## Standing commands

Run the repository's configured targeted GTest/CTest entries for type inference, lowering, codegen, external runtime, native interop, security, feature/pipeline, and sanitizer coverage. Then run:

```powershell
python scripts/syntax-convergence-gate.py
python scripts/docs-index.py --check
python scripts/docs-lifecycle.py validate
python scripts/local-info-leak-gate.py
python <better-plan-skill>/scripts/manifest_tool.py validate docs/plan
```

`docs-audit.py` is also required for final acceptance; unrelated pre-existing failures must be recorded separately and may not be hidden by weakening this plan's checks.

## Acceptance record

Final validation records command, platform/configuration, result, and the exact test or artifact satisfying each `REQ-UZ-*` label. A source change after evidence capture invalidates the affected record and routes back to the owning implementation node.
