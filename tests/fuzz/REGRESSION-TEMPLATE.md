# Fuzz Regression Template

- **Failure Date:** YYYY-MM-DD
- **Detected By:** nightly / CI / local
- **Target:** `styio_fuzz_lexer` or `styio_fuzz_parser`
- **Owner:** @owner

## 1. Symptom

- Fuzzer exit signal/code:
- Sanitizer class:
- Top stack frame:

## 2. Reproduction

```bash
# 1) Build fuzz targets
cmake -S . -B build-fuzz -DSTYIO_ENABLE_FUZZ=ON
cmake --build build-fuzz --target styio_fuzz_lexer styio_fuzz_parser

# 2) Replay one artifact
./build-fuzz/bin/<fuzz-target> <artifact-file> -runs=1
```

- Platform / compiler:
- Sanitizer options:
- Input artifact SHA256:

## 3. Corpus Backflow

- Case pack path: `fuzz-regressions/<run-id>/`
- Selected seed copied to: `tests/fuzz/corpus/<lexer|parser>/`
- Why this seed is representative:

## 4. Root Cause

- Subsystem:
- Trigger path:
- Why previous tests missed it:

## 5. Fix

- Fix PR/commit:
- Behavior change summary:
- Risk/tradeoff:

## 6. Regression Coverage

- Added/updated tests:
- Fuzz smoke command:
- Deterministic test command:

## 7. Follow-ups

1.
2.
