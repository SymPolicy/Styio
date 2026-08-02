# Runtime Value Interface Validation

**Purpose:** Define validation requirements for runtime value interfaces and ownership.

**Last updated:** 2026-08-02

## Requirements

- `REQ-RUNTIME-VALUE-INTERFACES`: Styio exposes natural string character
  materialization and maintains one valid mutable-list owner across function
  arguments, loop captures, returns, and subsequent caller use.

## Acceptance

- String input reaches ordered `list[char]` values without integer source
  encoding.
- Mutable list arguments remain valid for their caller after a function call.
- A list constructed through loop mutation can be returned, indexed, and
  passed onward.
- Focused active-handle evidence shows neither leaks nor double release.
- The downstream Brainfuck example uses the accepted interfaces without an
  old compatibility protocol.

## Focused compiler checks

Build only the affected compiler and test targets while implementation is in
progress:

```sh
cmake --build build -j2 --target styio styio_test styio_codegen_internal_test
ctest --test-dir build --output-on-failure \
  -R '^(stdio_input_|inferred_generics_|StyioCodegenInternal|StyioFeatureIntegration)'
```

Add `t09_string_chars` coverage for empty, ASCII, and UTF-8-byte ordering. The
fixture must type-check `list[char]`, preserve embedded non-command bytes, and
show that the result can be indexed and iterated.

Add `t08_list_handle_lifetime` coverage with user-defined functions that:

1. receive a list parameter, mutate it inside a loop, return normally, and
   leave the caller's handle valid;
2. allocate and mutate a list inside a loop, return it, and let the caller
   index it and pass it to another function;
3. return a borrowed list parameter and keep both the original caller value
   and returned value valid;
4. exercise normal iteration exit, `break`, and `continue` without double
   release.

Where the runtime exposes active-handle counters, assert that each case returns
to its pre-test count. Otherwise use the existing leak/error instrumentation
and assert a clear runtime error channel after execution.

## Downstream acceptance

After compiler checks pass, build and run the Brainfuck example using the
freshly built `styio-nightly/build/bin/styio`:

- compilation and execution of a known program that prints `42`;
- nested loops and pointer movement;
- source containing spaces/comments and original line breaks flattened by the
  launcher;
- optional numeric input supplied after the source line;
- unmatched bracket diagnostics;
- proof by source search that the integer-byte envelope and workaround text no
  longer exist.

## Final regression

Run the impacted compiler regression once after all compiler and downstream
changes are complete. A full repository regression is reserved for the final
state and is not repeated during implementation:

```sh
cmake --build build -j2 --target styio styio_test \
  styio_codegen_internal_test styio_externlib_test
ctest --test-dir build --output-on-failure \
  -R '^(stdio_input_|inferred_generics_|StyioCodegenInternal|StyioFeatureIntegration|StyioExternLib)'
```

Record the exact commands, exit status, and any consciously unrun broader test
suite in the node validation evidence.
