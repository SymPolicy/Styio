# Runtime Value Interface Evidence

**Purpose:** Record implementation and acceptance evidence for runtime value interfaces.

**Last updated:** 2026-08-02

## Reviewer repairs

- Collection return lowering now moves an owned temporary or owned dynamic
  local out of its cleanup set. Borrowed collection parameters are still
  cloned once so the caller receives an independent owned result.
- Generated task functions isolate the new collection-temporary scope and
  return-family bookkeeping from their caller's code-generation state.
- Resource-topology type hints recognize `string.chars()` as `list[char]`.
- List, dict, and matrix active-handle counters increment only after a handle
  is successfully registered.
- The runtime oracle reads returned list, dict, and matrix handles before
  checking that all three active counters return to baseline. The language
  fixture also covers a collection parameter whose name matches an outer
  dynamic collection binding.

## Bounded reviewer diagnostics

The Reviewer ran only affected build and individual-test diagnostics. These
are intentionally not the frozen final regression.

```sh
cmake --build build -j2 --target styio styio_codegen_internal_test styio_resource_topology_test
# exit 0

build/bin/styio_codegen_internal_test --gtest_filter='StyioRuntimeValueInterfaces.*'
# exit 0; 2 tests passed

ctest --test-dir build --output-on-failure \
  -R '^(inferred_generics_t08_list_handle_lifetime|stdio_input_t09_string_chars)$'
# exit 0; 2 tests passed

git diff --check
# exit 0
```

The `t09_string_chars` fixture observes an empty list for an empty input line,
ordered ASCII characters including spaces, an indexable `+`, iteration in
stored order, and the two UTF-8 bytes `\xC3` and `\xA9`. The
`t08_list_handle_lifetime` fixture observes caller validity after borrowed
mutation, use of a loop-built returned list, independent ownership when a
borrowed parameter is returned, cleanup through normal/`break`/`continue`
paths, and correct raw-ABI parameter loading despite a same-named outer
collection slot.

## Final acceptance

After the Reviewer returned, the Better Plan final-validation transition ran
the frozen regression exactly once:

```sh
cmake --build build -j2 --target styio styio_test \
  styio_codegen_internal_test styio_externlib_internal_test

ctest --test-dir build --output-on-failure \
  -R '^(stdio_input_|inferred_generics_|StyioCodegenInternal|StyioExternlibInternal|StyioFeatureIntegration)'
```

Both commands exited 0. The acceptance receipt is recorded in
`Checkpoints.json` with the final content fingerprint.

The downstream `styio-example/brainfuck-compiler` was then run against the
freshly built compiler. Its acceptance script passed the `42` program, nested
loops, numeric input, run compression, ASCII and multiline UTF-8 comments,
unmatched-bracket diagnostics, and the negative-pointer diagnostic. A one-time
migration search found no remaining source-length/byte envelope or workaround
text, and `git diff --check` passed for the example directory.
