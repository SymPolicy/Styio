# Checkpoint Health

**Purpose:** Define the repository-wide build/test health entrypoint for `styio-nightly` so CI and checkpoint delivery can call one script instead of wiring compiler-specific checks inline.

**Last updated:** 2026-04-19

## Goal

`scripts/checkpoint-health.sh` is the inner health gate for this repository. It owns configure/build steps, the compiler test labels, and repo-specific parser/security/soak verification needed at checkpoint scope.

When `--asan-build-dir` points at a missing directory, the script now bootstraps a RelWithDebInfo ASan/UBSan configure in that location before running the sanitizer leg.

## Command

Default checkpoint health:

```bash
./scripts/checkpoint-health.sh
```

Fast local checkpoint health:

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

## What It Runs

At the outer interface, callers only need to know that this script:

1. configures or reuses the normal build directory
2. builds the required compiler/test targets
3. runs the docs, pipeline, security, parser, and soak legs needed for checkpoint health
4. optionally runs ASan/UBSan and fuzz smoke when requested

The exact internal test labels and target names may evolve, but CI and delivery scripts should continue to call this file rather than inline its implementation details.
