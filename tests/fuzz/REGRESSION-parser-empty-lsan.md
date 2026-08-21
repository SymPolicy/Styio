# Parser Fuzz Regression: Process-Exit Leak Aggregate

- **Failure Date:** 2026-08-15 and 2026-08-19
- **Detected By:** nightly CI runs `31862492601` and `32213494185`
- **Target:** `styio_fuzz_parser`
- **Owner:** compiler-signal-green

## 1. Symptom

- Fuzzer exit signal/code: non-zero at process shutdown
- Sanitizer class: LeakSanitizer
- Top stack frame: parser-owned string/AST allocations retained after malformed-input exception paths

The emitted zero-byte artifact is a libFuzzer process-exit placeholder for leaks accumulated while running the corpus. It is not, by itself, the unique triggering input.

## 2. Reproduction

```bash
cmake -S . -B build/fuzz -DSTYIO_ENABLE_FUZZ=ON
cmake --build build/fuzz --target styio_fuzz_parser
./build/fuzz/bin/styio_fuzz_parser tests/fuzz/corpus/parser -max_total_time=600
```

- Platform / compiler: nightly Linux x86_64 sanitizer build
- Sanitizer options: AddressSanitizer with LeakSanitizer process-exit detection
- Input artifact SHA256: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`

## 3. Corpus Backflow

- Case pack paths: `fuzz-regressions/gh-31862492601-1/` and `fuzz-regressions/gh-32213494185-1/`
- Selected seed copied to: `tests/fuzz/corpus/parser/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.seed`
- Why this seed is representative: it preserves the exact artifact identity emitted by both process-exit leak reports and keeps empty-input handling in the permanent corpus.

## 4. Root Cause

- Subsystem: legacy parser ownership on malformed-input exception paths
- Trigger path: partial resource lists, braced/parenthesized paths, and forwarded blocks could throw after allocating AST nodes without an owner
- Why previous tests missed it: deterministic tests checked diagnostics and syntax behavior but did not retain a sanitizer process long enough to expose aggregate exception-path leaks

## 5. Fix

- Fix PR/commit: PLAN-001 / TASK-001 workspace change
- Behavior change summary: parser construction paths now hold partial AST nodes in `std::unique_ptr` containers and release ownership only after syntax validation succeeds
- Risk/tradeoff: no accepted syntax or diagnostic contract changes; ownership transfer points are narrower and exception-safe

## 6. Regression Coverage

- Added/updated tests: parser internal malformed-resource/path/block edges and permanent empty seed
- Fuzz smoke command: `./build/fuzz/bin/styio_fuzz_parser tests/fuzz/corpus/parser -runs=1`
- Deterministic test command: `build/default/bin/styio_parser_internal_test`

## 7. Follow-ups

1. Re-run the nightly Linux sanitizer fuzz job to validate process-exit leak freedom.
2. Keep malformed-input constructors RAII-owned until their complete parse succeeds.
