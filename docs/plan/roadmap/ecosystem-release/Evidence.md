# Ecosystem Release Evidence

**Purpose:** Record the maintainer Styio-only release-scope decision and the sole acceptance gate for this candidate.

**Last updated:** 2026-08-03

## Frozen scope

1. Included capabilities: the current Styio functional baseline already delivered in completed Manifest Plans.
2. Explicit exclusions: cross-repository ecosystem matrix; Pafio, Platform, and Vityo participation; any wait on consumer repositories.
3. Participating repositories: `styio-nightly` only.
4. Candidate-freeze condition: the Styio revision under test; acceptance is one Styio repo-local full functional regression.

## Sole acceptance gate

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

This is the repository-local full functional regression for the Styio-only candidate. Cross-repository matrix work remains skipped (`RELEASE-MATRIX`).

## Confirmation run

1. Plan state: `RELEASE-SCOPE` completed; `RELEASE-MATRIX` skipped with maintainer waiver; Ecosystem Release Plan completed.
2. Docs leg of the sole gate: `ctest --test-dir build/default -L docs --output-on-failure --no-tests=error` passed after syncing `docs/plan/INDEX.md`, `docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md`, and `docs/teams/DOC-STATS.md`.
3. Full sole-gate run (`./scripts/checkpoint-health.sh --no-asan --no-fuzz`) failed at the pipeline/security leg: 23 of 352 tests failed. Docs and core-benchmark smoke legs passed. The failures are Styio-owned pipeline debt isolated from this docs/plan closure (five-layer goldens, resource-method range clone, parser pointer-match diagnostic, and multiple `StyioResourceEffects` value/slice/stdin cases). They do not reopen ecosystem matrix participation.
