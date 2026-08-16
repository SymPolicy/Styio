# Runtime Value Interfaces

**Purpose:** Define runtime value interfaces and collection ownership across function boundaries.

**Last updated:** 2026-08-02

## Intent

Define the smallest parser-through-runtime repair that lets Styio programs
consume source text naturally and preserves runtime collection ownership across
user-defined function boundaries. The first downstream acceptance case is the
Brainfuck compiler in `styio-example`.

## Scope and constraints

- Preserve generation-checked runtime handles and fail-closed diagnostics.
- Preserve the existing line-oriented `string := (<< @stdin)` stream contract.
- Use bounded linear character materialization.
- Repair collection ownership generically for `list`, `dict`, and `matrix`
  wherever the IR already treats those values as runtime handles.
- Remove the downstream Brainfuck integer-source transport after compiler
  acceptance passes; no compatibility path remains.
- Do not redesign CLI argument syntax, stream scheduling, or unrelated
  container operations.

## Public string interface

Add the built-in method:

```styio
string.chars() -> list[char]
```

`chars()` materializes one `char` for every byte in the string's stored order.
Styio's current `char` representation is eight bits, so the operation exposes
UTF-8 bytes rather than Unicode scalar values. Empty input produces an empty
list. Runtime cost is O(n) time and O(n) additional storage, with a single
result allocation and no repeated string slicing.

This deliberately does not change stdin behavior. A scalar string pull remains
one line, excluding the line terminator. Programs that need a textual program
plus later input values can consume the source on the first line and consume
the remaining lines independently.

The compiler path is complete end to end: built-in method registration, type
inference, AST-to-IR lowering, generated runtime declaration, runtime
implementation, JIT symbol registration, and returned-handle tracking.

## Runtime collection ownership

Runtime handles have explicit ownership even though the Styio surface syntax
does not expose it.

### Function parameters

Collection parameters are borrowed for the duration of the call. The callee
may read and mutate the referenced collection, but must not release the
argument handle. The caller retains its pre-call ownership state. Passing an
owned temporary as an argument does not consume it.

### Local values and temporaries

A newly allocated collection is owned by the current function activation.
Compiler bookkeeping records it exactly once. Normal scope exits and early
control-flow exits release owned temporaries that do not escape. Borrowed
parameters are never entered into this owned set.

Mutation methods return or reuse a handle without creating a second owner.
Replacing the handle stored for a local variable transfers the existing local
ownership to the replacement; it must not release the receiver before the
mutation result is installed.

### Function returns

Returning a collection transfers one ownership unit from callee to caller.
Before emitting the return, the callee removes the returned handle from its
cleanup set, then cleans all other owned collection temporaries. A borrowed
parameter returned directly is retained once so that the caller receives an
owned result while the original caller-side owner remains valid.

After a user-defined call whose declared result is `list`, `dict`, or `matrix`,
the caller records the returned handle as an owned temporary. Assignment or a
subsequent return may then transfer that ownership in the usual way.

### Loops and control flow

Loop bodies borrow captured collection locals and parameters. Mutations target
the same generation-checked handle; loop entry does not clone or consume it.
Iteration-local owned temporaries are released once on normal iteration exit,
`break`, and `continue`. A value transferred through `return` is excluded from
those cleanup paths.

## Failure behavior

Null, stale, or generation-mismatched handles continue to set the runtime error
channel and fail closed. The repair must not make an invalid handle appear
empty or silently allocate a replacement. Allocation failure from `chars()` is
reported through the existing runtime error mechanism.

## Pattern assessment

- `pattern_catalog`: `refactoring-guru-catalog-22-v1`
- `candidate`: `none`
- `decision`: `reject`
- `pressure`: The compiler needs one consistent ownership-transfer rule at
  function calls and returns plus one direct string materialization primitive.
- `reason`: Direct lowering helpers and runtime functions express these rules
  without introducing interchangeable families or object wrappers. Adapter,
  Proxy, Strategy, and similar catalog patterns would add indirection without
  changing either the ABI or the ownership invariant.

## Downstream handoff

Only after the compiler acceptance suite passes, migrate the Brainfuck example
as one complete replacement:

```styio
source: string := (<< @stdin)
symbols: list[char] := source.chars()
bytecode: list[i64] := compile_brainfuck(symbols)
run_bytecode(bytecode)
```

The launcher writes the Brainfuck source as one line, replacing embedded CR and
LF characters with spaces. This preserves Brainfuck semantics because all
non-command characters are comments. Optional VM input values follow on later
lines. The old source-length and integer-byte envelope, its documentation, and
its tests are removed together.
