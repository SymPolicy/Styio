# Parser Fuzz Regression: Forward Iterator Timeout

- **Failure Date:** 2026-08-18
- **Detected By:** nightly CI run `32096745697`
- **Target:** `styio_fuzz_parser`
- **Owner:** compiler-signal-green

## 1. Symptom

- Fuzzer exit signal/code: libFuzzer timeout after 1555 seconds
- Sanitizer class: libFuzzer timeout watchdog
- Top stack frame: `StyioContext::cur_tok()` while repeatedly checking an unconsumed iterator token

## 2. Reproduction

```bash
cmake -S . -B build/fuzz -DSTYIO_ENABLE_FUZZ=ON
cmake --build build/fuzz --target styio_fuzz_parser
./build/fuzz/bin/styio_fuzz_parser \
  tests/fuzz/corpus/parser/e8566dab795531219782e5a24a2bda31e1d3fbef47ef15b7e207b275f375bfb6.seed \
  -runs=1 -timeout=30
```

- Platform / compiler: nightly Linux x86_64 sanitizer build
- Sanitizer options: libFuzzer timeout watchdog
- Input artifact SHA256: `e8566dab795531219782e5a24a2bda31e1d3fbef47ef15b7e207b275f375bfb6`

## 3. Corpus Backflow

- Case pack path: `fuzz-regressions/gh-32096745697-1/`
- Selected seed copied to: `tests/fuzz/corpus/parser/e8566dab795531219782e5a24a2bda31e1d3fbef47ef15b7e207b275f375bfb6.seed`
- Why this seed is representative: this exact 60-byte artifact reaches the forward-list boundary with an iterator token and reproduced the watchdog timeout.

## 4. Root Cause

- Subsystem: legacy and nightly forward-list parser helpers
- Trigger path: `parse_forward_as_list` and `parse_forward_as_list_nightly_draft` saw `ITERATOR`, declined to parse it, and looped without advancing the cursor
- Why previous tests missed it: forward-list tests covered valid continuations but did not assert bounded decline behavior when the next construct belonged to the iterator parser

## 5. Fix

- Fix PR/commit: PLAN-001 / TASK-001 workspace change
- Behavior change summary: both forward-list helpers return immediately on `ITERATOR`, leaving the token unconsumed for the owning iterator parser
- Risk/tradeoff: preserves parser ownership boundaries and valid iterator parsing while guaranteeing cursor progress or return

## 6. Regression Coverage

- Added/updated tests: `StyioParserInternal.UnifiedOperatorForwardAndCodpEdgesStayExplicit` and `StyioNewParserInternal.ForwardIteratorAndContinuationEdgesStayExplicit`
- Fuzz smoke command: `./build/fuzz/bin/styio_fuzz_parser tests/fuzz/corpus/parser -runs=1`
- Deterministic test command: `build/default/bin/styio_parser_internal_test && build/default/bin/styio_newparser_internal_test`

## 7. Follow-ups

1. Re-run the nightly Linux fuzz job with the imported seed.
2. Require every parser loop to consume input, transfer control, or return on each iteration.
