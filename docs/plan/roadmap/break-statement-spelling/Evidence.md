# Minimum Break Spelling Evidence

**Purpose:** Record accepted evidence for standalone break spelling behavior.

**Last updated:** 2026-08-02

## Accepted syntax contract

- A standalone `^` is rejected by both parser routes at the original caret
  with the stable minimum-length diagnostic.
- `^^` is the shortest legal spelling, `^^^` is conventional, and longer
  contiguous runs have no fixed maximum.
- Runs of 2, 3, 4, and 64 carets each produce exactly one `BreakAST` and one
  `SGBreak`, both normalized to depth `1`.
- Spaces and comments never join runs. One-caret runs separated by trivia
  retain the minimum-length diagnostic; two legal runs in one logical
  statement receive the separated-run diagnostic. A newline produces two
  independent depth-1 break statements.

## Isolation and migration

Focused AST assertions retain bitwise XOR and the established list index,
removal, and multi-value caret operations. The parser implementation changed
only the two existing statement-start break paths; lexer, AST shape, IR shape,
lowering, and code generation remain unchanged.

The active-source audit found no isolated single caret in repository `.styio`
inputs or the canonical Brainfuck source. Authored breaks use `^^^`, except
for the intentional `^^^^` long-run fixture. The Brainfuck compiler's three
break sites were migrated together.

The EBNF is the spelling SSOT and now defines
`BREAK_TOKEN = '^' '^' { '^' }`. The language design and symbol reference
repeat the same minimum, convention, unbounded-length, contiguity, and
nearest-loop rules.

## Regression evidence

The focused acceptance receipt records successful compiler/test builds, all
three `BreakSpelling` tests, and the isolated-caret migration audit. The one
group Reviewer additionally confirmed exact diagnostic parity, AST/IR
normalization, context isolation, downstream migration, and documentation
consistency, with no blocker or developer decision.

The final-validation receipt is authoritative for the single impacted
regression: `BreakSpelling`, `control_flow`, `stdio_input`, and
`inferred_generics`, the canonical Brainfuck `make test`, and
`git diff --check` must all exit successfully against the freshly built
nightly compiler.
