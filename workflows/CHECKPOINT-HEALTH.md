# Checkpoint Health

**Purpose:** Define the repository-wide build/test health entrypoint for `styio-nightly` so CI and checkpoint delivery can call one script instead of wiring compiler-specific checks inline.

**Last updated:** 2026-06-28

## Goal

`scripts/checkpoint-health.sh` is the inner health gate for this repository. It owns configure/build steps, the compiler test labels, and repo-specific parser/security/soak verification needed at checkpoint scope.

For functional changes, this gate is intentionally after the [FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md](./FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md) self-check and, when behavior is replaced or migrated, after the [FEATURE-CUTOVER-WORKFLOW.md](./FEATURE-CUTOVER-WORKFLOW.md) self-check. The changed feature must have targeted validation, upstream/downstream adaptation, version-style naming checked against the feature or transformation result, and objective unable-to-verify records before final tests are treated as complete. The script prints those reminders before its build/test legs.

When `--asan-build-dir` points at a missing directory, the script now bootstraps a RelWithDebInfo ASan/UBSan configure in that location before running the sanitizer leg.

Build parallelism is controlled by `--build-jobs`, `STYIO_CHECKPOINT_BUILD_JOBS`, or `CMAKE_BUILD_PARALLEL_LEVEL`. If none is set, the script caps parallelism by detected CPU count and total memory so cold builds on smaller CI workers do not fail before reaching CTest. The same job count is forwarded to the coverage leg.

The health gate also runs `scripts/coverage-gate.sh` with a default 95% project
source line-coverage threshold. A coverage result below 95% fails checkpoint
health and therefore fails the delivery gate unless the delivery is explicitly
docs/process-only and uses `--skip-health`.

## Command

Default checkpoint health:

```bash
./scripts/checkpoint-health.sh
```

Fast local checkpoint health:

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

Memory-constrained worker:

```bash
./scripts/checkpoint-health.sh --build-jobs 2
```

Coverage build override:

```bash
./scripts/checkpoint-health.sh --coverage-build-dir build/coverage --coverage-threshold 95
```

## What It Runs

At the outer interface, callers only need to know that this script:

1. configures or reuses the normal build directory
2. builds the required compiler/test targets
3. runs the docs, pipeline, security, parser, and soak legs needed for checkpoint health
4. runs the coverage gate and fails below 95% project source line coverage
5. optionally runs ASan/UBSan and fuzz smoke when requested

It does not prove commit readiness or cutover by itself; the caller must complete the commit-readiness self-check for functional changes and the feature-cutover self-check when the change replaces or migrates existing behavior.

The exact internal test labels and target names may evolve, but CI and delivery scripts should continue to call this file rather than inline its implementation details.
