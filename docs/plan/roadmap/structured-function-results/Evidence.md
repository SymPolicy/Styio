# Structured Function Results Evidence

**Purpose:** Record accepted implementation evidence for structured function results.

**Last updated:** 2026-08-02

## Accepted source contract

```styio
# compile : (i64, i64, list[i64]) := (source: list[char]) => {
    <| (status, detail, bytecode)
}

result := compile(source)
status := result[0]
detail := result[1]
bytecode: list[i64] := result[2]
```

The tuple is one immutable, structurally typed value. Its generation-checked
runtime handle owns nested collection values. Projecting a collection clones
it for the caller, so releasing the tuple cannot invalidate the projection.
Focused active-handle tests return tuple and list counters to their baselines
and reject stale tuple handles.

## Downstream migration

The Brainfuck compiler returns `status`, `detail`, and `bytecode` as separate
tuple elements. The VM receives only `bytecode`, addresses interleaved
`[opcode, operand]` pairs from index zero, and contains no compatibility path
for the removed two-cell status header. A source search confirmed that the
remaining `bytecode[1 + opening * 2]` access is the operand slot of a zero-based
jump instruction.

## Regression evidence

All commands completed with exit status 0:

- built `styio`, `styio_test`, `styio_codegen_internal_test`, and
  `styio_lowering_internal_test`;
- `ctest --test-dir build -L inferred_generics --output-on-failure` — 13/13;
- `ctest --test-dir build -L control_flow --output-on-failure` — 13/13;
- canonical Brainfuck compiler `make test` using the freshly built nightly
  compiler;
- `StyioStructuredFunctionResults.*` — 4/4, including nested-list ownership
  and expression-match region-yield verification;
- callable canonical-identity regression; and
- `git diff --check`.

The group Reviewer additionally repaired tuple-child traversal in dependency
analysis, canonicalization, and constant folding, while preserving canonical
equality between name-parsed callable types and equivalent callables carrying
cached structural metadata.
