# IM-D6 Release Conformance Inventory

**Purpose:** Record the accepted release, conformance, and regression-matrix decisions for IM-D6 so nightly promotion is governed by explicit gate tiers instead of ad hoc "run more tests" guidance.

**Last updated:** 2026-06-05

## Scope

IM-D6 owns the promotion-quality contract for this repository:

- which gates are always required,
- which gates are required only when an affected area changes,
- which gates belong to nightly or release-candidate validation,
- which lanes are advisory or pending until infrastructure exists,
- how skipped lanes are recorded, and
- when a failed or unavailable lane blocks promotion.

IM-D6 does not define new Styio language semantics. It also does not move package lifecycle ownership from `pafio-nightly` into this compiler repository. The matrix decides evidence and release readiness, not feature meaning.

## Current State

The repository already has strong local gate entrypoints:

- [Unified Delivery Gate](../../workflows/DELIVERY-GATE.md) owns the delivery floor.
- [Checkpoint Health](../../workflows/CHECKPOINT-HEALTH.md) owns build/test health.
- team runbooks own domain-specific checks for parser, Sema/IR, runtime, IDE/LSP, tests, and performance.
- `styio-audit` provides external audit policy.

The gap is not "add one more big script." The gap is to classify when each gate is required, advisory, pending, skipped with a reason, or promotion-blocking.

## Gate Tier Model

Accepted decision:

Styio uses four release-conformance tiers.

| Tier | Name | Trigger | Purpose |
|------|------|---------|---------|
| L0 | Commit gate | Local commit or staged delivery | Keep obviously invalid repository state out of history |
| L1 | Push / PR gate | Push, PR, or branch handoff | Prove the changed branch is deliverable on the main supported path |
| L2 | Affected-area gate | Required when matching source, tests, or docs change | Run the heavier checks owned by the changed subsystem |
| L3 | Nightly / release gate | Scheduled nightly, promotion, or release candidate | Run the full quality matrix, including expensive or infrastructure-heavy lanes |

Routine work uses the lowest tier that covers the changed surface. Promotion and release-candidate records must list the full matrix result or explicitly record pending lanes.

## Lane Status Vocabulary

Accepted decision:

Every lane in the release matrix has a status.

| Status | Meaning |
|--------|---------|
| `required` | The lane must pass for the tier where it is required |
| `affected-required` | The lane is required only when its owned surface changed |
| `advisory` | The lane should run and report evidence, but failure does not block the current tier |
| `pending` | The lane is not yet enforceable because infrastructure, runners, fixtures, or external repos are missing |
| `skip-with-reason` | The lane did not run in this environment and must report the exact reason |

`skip-with-reason` is not success. It is acceptable only when the lane is not required for the current tier, or when the release record explicitly accepts the debt as pending.

## Block Policy Vocabulary

Accepted decision:

Every lane also has a block policy.

| Block policy | Meaning |
|--------------|---------|
| `block` | Failure blocks the current tier |
| `block-if-affected` | Failure blocks when the changed file range touches the lane's owned surface |
| `record-debt` | Failure or absence is recorded as pending release debt but does not block the current tier |
| `manual-review` | Human release owner must decide whether the evidence is sufficient |

A required lane that cannot run must be treated as blocked unless the release record explicitly downgrades it to pending with a reason and owner.

## L0 Commit Gate

Accepted baseline:

| Lane | Status | Block policy | Evidence |
|------|--------|--------------|----------|
| Repo hygiene for staged/worktree changes | `required` | `block` | `scripts/repo-hygiene-gate.py` |
| Generated docs index freshness | `required` when docs change | `block-if-affected` | `python3 scripts/docs-index.py --check` |
| Docs audit | `required` when docs change | `block-if-affected` | `python3 scripts/docs-audit.py` |
| Team runbook gate | `required` when owned surfaces change | `block-if-affected` | `python3 scripts/team-docs-gate.py --mode staged` |
| Runtime surface registry | `required` when runtime/codegen surface changes; allowed in staged delivery floor | `block-if-affected` | `python3 scripts/runtime-surface-gate.py` |

L0 is intentionally small. It prevents invalid repository shape and stale generated documentation without turning every commit into a full release candidate.

## L1 Push / PR Gate

Accepted baseline:

| Lane | Status | Block policy | Evidence |
|------|--------|--------------|----------|
| Unified delivery gate push profile | `required` | `block` | `./scripts/delivery-gate.sh --mode push ...` |
| External audit | `required` for branch handoff and downstream promotion | `block` | `styio-audit gate --repo . --project styio` through delivery gate |
| Checkpoint health fast lane | `required` for non-doc delivery | `block` | `./scripts/checkpoint-health.sh --no-asan --no-fuzz` |
| Project source line coverage >= 95% | `required` for non-doc delivery | `block` | `scripts/coverage-gate.sh --build-dir build/coverage --threshold 95` through checkpoint health |
| Service contract smoke | `affected-required` for `src/StyioServices/**`, CLI, IDE, LSP, contract JSON changes | `block-if-affected` | focused service tests and documented machine-readable outputs |
| CTest baseline labels | `affected-required` for compiler/test changes | `block-if-affected` | labels from the owning team runbook |

Docs-only changes may use the documented `--skip-health` path when the delivery record shows the change did not touch executable compiler behavior.

## L2 Affected-Area Gates

Accepted affected-area matrix:

| Changed area | Required gates |
|--------------|----------------|
| Parser, lexer, grammar authority | parser shadow gate, syntax fixtures, syntax-check diagnostics, parser fuzz smoke or corpus replay |
| Sema, lowering, StyioIR verifier | five-layer pipeline, verifier unit tests, semantic negative tests, codegen gate rejection tests when applicable |
| Codegen, runtime helpers, JIT, handle table | runtime surface gate, relevant CTest labels, ASan/UBSan focused leg, security/resource tests |
| Resource topology and resource effects | resource topology tests, semantic negatives, fallback/discard diagnostics, cleanup and pressure tests |
| Stream runtime, scheduler, task resources | task scheduler tests, stream/zip/backpressure fixtures, perf quick route, soak smoke |
| Native interop | native feature fixtures, artifact build/run tests, ABI smoke, sanitizer focused leg |
| StyioServices CLI / IDE / LSP / Contract | public CLI smoke, JSON contract tests, README/manifest alignment, LSP or IDE capability tests |
| Package/nano compiler contracts | nano producer/verifier tests, compile-plan negative paths, service manifest checks |
| Docs and runbooks | docs index, docs audit, team docs gate, lifecycle/index checks |
| Performance thresholds or benchmark routes | perf quick route, relevant benchmark report, Test Quality review if promoted to required gate |

If a change touches multiple areas, the required gate set is the union of the affected lanes. The owner may narrow the run only when the release record explains why a listed lane is unrelated despite the path match.

## L3 Nightly / Release Gate

Accepted nightly/release matrix:

| Lane | Nightly status | Release-candidate status | Block policy |
|------|----------------|--------------------------|--------------|
| L0 + L1 complete delivery floor | `required` | `required` | `block` |
| Full checkpoint health | `required` | `required` | `block` |
| ASan/UBSan | `required` on Linux x86_64 | `required` on Linux x86_64 | `block` |
| Fuzz corpus replay / fuzz smoke | `required` | `required` | `block` |
| Project source line coverage >= 95% | `required` | `required` | `block` |
| Deep libFuzzer run | `advisory` nightly | `advisory` or scheduled release evidence | `record-debt` unless promoted |
| Release perf quick route | `required` for runtime/codegen/scheduler/native deltas | `required` | `block-if-affected` for nightly, `block` for release candidate |
| Soak smoke | `required` for runtime/resource/stream/scheduler deltas | `required` | `block-if-affected` for nightly, `block` for release candidate |
| Cross-platform build | `pending` for macOS/Windows until stable runners exist | `manual-review` until stable runners exist | `record-debt` |
| Linux x86_64 build/test | `required` | `required` | `block` |
| Conformance fixture matrix | `required` for accepted language/service surfaces | `required` | `block` |
| Package manager full UX | out of scope for this repo; track as `pafio-nightly` dependency | out of scope for this repo; track as `pafio-nightly` dependency | `record-debt` only when compiler contract evidence is missing |

The initial required platform is Linux x86_64. macOS and Windows remain tracked pending/advisory lanes until the repository owns stable runners and documented toolchain setup for them.

## Conformance Matrix

Accepted decision:

Conformance is organized by feature and service contract, not by a single undifferentiated test bucket.

Required conformance families:

1. syntax-only parser contract,
2. public diagnostics JSON/JSONL contract,
3. StyioServices CLI/IDE/LSP/Contract manifest and README behavior,
4. feature fixtures under `tests/features/`,
5. semantic negative tests with stable diagnostic fragments,
6. five-layer parser/type/IR/LLVM/runtime evidence where lowering is accepted,
7. resource topology and resource-effect behavior,
8. native interop syntax/build/run fixtures,
9. compile-plan and nano compiler contract negative paths, and
10. release/performance evidence for runtime-affecting changes.

Accepted behavior requires positive evidence. Rejected behavior requires a stable negative diagnostic. A missing conformance family is not green; it is either `pending` with an owner or blocks the promotion record for that surface.

## Package Boundary

Accepted decision:

This repository owns compiler-side package and service contracts only:

- nano producer/verifier behavior,
- compile-plan contracts,
- package-facing machine-readable metadata,
- negative-path validation for compiler-owned artifacts, and
- service docs/manifests that describe those contracts.

Project creation, manifests, lockfiles, dependency resolution, vendoring,
packaging, registry trust on the client, and publishing belong to
`pafio-nightly`. Registry service operation, remote authentication, hosted
workspaces, and workers belong to Styio Platform.

## Promotion Record Requirements

Accepted decision:

A nightly or release-candidate record must include:

1. commit SHA and branch,
2. requested tier: L1, L2, nightly, or release candidate,
3. platform and toolchain,
4. lane table with status, command, result, and artifact path when available,
5. every skipped lane with reason,
6. every pending lane with owner and follow-up,
7. every advisory failure with risk classification, and
8. final promotion decision: pass, blocked, or pass-with-pending-debt.

"Pass-with-pending-debt" is allowed for nightly only when no required lane failed and the pending lane is explicitly non-required for the current tier. It is not a release-candidate default.

## Stop Condition

IM-D6 can close only when:

1. the L0/L1/L2/L3 tier model is implemented or referenced by the delivery workflow,
2. all existing delivery/checkpoint/audit entrypoints map to a tier,
3. affected-area gates are documented by owning team,
4. nightly/release records can express `required`, `affected-required`, `advisory`, `pending`, and `skip-with-reason`,
5. unsupported or unavailable lanes are visible as pending debt instead of silent success,
6. Linux x86_64 required lanes pass for the requested promotion tier,
7. conformance families have positive and negative evidence or explicit pending owners,
8. project source line coverage is at least 95% for required Linux x86_64 lanes, and
9. cross-platform, deep fuzz, and full perf lanes are either operational or explicitly tracked as advisory/pending.

Until then, a promotion may still happen only with a record that lists the missing matrix lanes as pending.

## Decision Closure

No IM-D6 release-matrix decision remains open in this inventory. Remaining work is implementation and CI/process integration: encode the matrix in delivery records, wire affected-area lane selection where useful, and add release evidence artifacts for nightly and release-candidate promotion.

## Source Documents

- [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md)
- [Unified Delivery Gate](../../workflows/DELIVERY-GATE.md)
- [Checkpoint Health](../../workflows/CHECKPOINT-HEALTH.md)
- [Test Quality Runbook](../teams/TEST-QUALITY-RUNBOOK.md)
- [Performance / Stability Runbook](../teams/PERF-STABILITY-RUNBOOK.md)
- [Docs / Ecosystem Runbook](../teams/DOCS-ECOSYSTEM-RUNBOOK.md)
