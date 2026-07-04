# Local Divergence Migration Ledger - 2026-07-04

**Purpose:** Track how locally preserved changes are being reintroduced on top of upstream `nightly` without restoring governance surfaces that upstream has deleted or renamed.

**Last updated:** 2026-07-04

## Baseline

1. Upstream source of truth: `upstream/nightly` as fetched on 2026-07-04.
2. Preserved local committed state: `codex/pre-upstream-governance-20260704-003620`.
3. Preserved local uncommitted state: `stash@{0}` with message `pre-upstream-governance-20260704-003620`.
4. Active realignment branch: `codex/upstream-governance-realign-20260704`.

## Governance Filters

1. Do not recreate the deleted `docs/plan/` long-form checkpoint tree.
2. Do not move active IDE/LSP code back under the deleted `src/StyioServices/` layout.
3. Keep compiler-side package contracts pointed at `styio-pafio`; do not restore `styio-spio` handoff paths in this repository.
4. Each migrated implementation slice must update its owning team runbook and the smallest matching tests.

## Migrated In This Slice

1. `src/main.cpp`: Windows-safe nano file URI handling, registry-root normalization, generated nano CMake dependency discovery, Windows output-name handling, and MSVC size-optimization guard.
2. `src/StyioJIT/StyioJIT_ORC.hpp`: native extern absolute symbols remain callable and exported.
3. `src/StyioIDE/Common.cpp`: Windows drive-letter URI decoding and slash-normalized URI emission are restored in the current IDE path.
4. `src/StyioNative/NativeInterop.cpp`: the dynamic-library loader now has a minimal Windows branch for process-local load, symbol lookup, unload, executable probing, temp directories, and process IDs without reintroducing the old platform module.
5. `src/StyioLSP/`: `initialized` is treated as a no-response notification, and `styio_lspd` switches stdio to binary mode on Windows.
6. `tests/`: focused LSP lifecycle and stdio framing regressions plus Windows-safe CTest registration for selected GTest slices.
7. `.github/workflows/styio-ci-gate.yml`: the Linux gate builds `styio_lspd` and runs the LSP transport smoke.

## Still Pending Migration

1. Large compiler feature branches from the preserved local history, including IR walker, verifier, scheduler/runtime state, parser/tokenizer rewrites, and broad codegen/lowering/type-inference changes.
2. Broad test additions tied to those feature branches. They must be migrated with their implementation owners rather than copied as orphan fixtures.
3. Local documentation inventories and old checkpoint manifests. Their surviving facts should be folded into this ledger, `NEXT-STAGE-GAP-LEDGER.md`, owning runbooks, implemented decision summaries, or active SSOTs.
4. Windows/macOS matrix expansion beyond the current upstream gate. This needs a dedicated CI checkpoint that keeps Pafio/View sibling checkout semantics intact.

## Required Proof For Future Slices

1. `python3 scripts/team-docs-gate.py`
2. `python3 scripts/docs-audit.py`
3. Targeted CTest label or regex for the owning implementation surface.
4. A short ledger update explaining which preserved local branch or stash item was absorbed and which governance filters were applied.
