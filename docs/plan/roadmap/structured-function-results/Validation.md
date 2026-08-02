# Structured Function Results Validation

**Purpose:** Define compiler and downstream validation for structured function results.

**Last updated:** 2026-08-02

## Compiler-focused acceptance

The focused suite must prove:

1. `(1, true, 'x', 2.5)` constructs and projects the declared scalar types.
2. A direct user function annotated `:(i64, list[i64])` returns a tuple and its caller can project and mutate the returned list.
3. A tuple returned from a loop-containing function keeps the nested list valid.
4. Repeated construction, return, projection, mutation, and scope exit restore tuple and list active-handle counters to baseline.
5. Tuple projection is O(1) and does not scan earlier elements.
6. String/collection projection produces an independent owned value that remains valid after tuple cleanup.

Negative fixtures must fail before codegen for:

- empty runtime tuples;
- return arity mismatch;
- return element-type mismatch with the failing index;
- dynamic and out-of-range projection indices;
- tuple parameters, indirect callable results, extern/native boundaries, mutation, iteration, and nested tuples in this slice; and
- any unshaped tuple IR reaching the verifier.

Focused commands build the affected compiler/test targets and run the inferred-generics tuple fixtures plus tuple-specific internal Sema, lowering, verifier, codegen, and active-handle tests.

## Downstream Brainfuck acceptance

The compiler function returns exactly:

```styio
(status, detail, bytecode)
```

The downstream source must contain no status/detail cells at the front of `bytecode`, no instruction offset of `2`, and no fallback compatibility path. `run_bytecode` receives only `list[i64]`; compile diagnostics use the separately projected `status` and `detail`.

Existing valid, comment, run-compression, balanced-loop, malformed-loop, nested-loop, tape-growth, and input-stream cases must continue to pass. Tests must additionally make a valid first opcode observable so an accidentally retained two-cell header fails.

## Final regression

After the single group Reviewer finishes, run once:

- affected compiler build targets;
- inferred-generics and control-flow feature labels;
- tuple-specific internal tests and ownership counters;
- canonical Brainfuck compiler tests using the freshly built nightly binary; and
- repository diff hygiene.

Evidence records the final tuple syntax, handle ownership baseline, downstream structured result, removal search for the old header protocol, and exact commands with exit status.
