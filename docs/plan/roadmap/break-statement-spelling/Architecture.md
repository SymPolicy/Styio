# Minimum Break Spelling Architecture

**Purpose:** Define the implementation boundary for minimum standalone break spelling.

**Last updated:** 2026-08-02

## Frozen capability boundary

This group changes only the spelling accepted when a statement begins with a
caret run. The lexer continues to emit one `TOK_HAT` per `^`; expression and
list parsers retain ownership of their existing caret contexts. The AST,
lowering, Styio IR, verifier, and code generator remain unchanged.

The language contract is:

| Source at a statement boundary | Result |
| --- | --- |
| `^` | Syntax error at that caret. |
| `^^` | The shortest legal break spelling. |
| `^^^` | The conventional spelling used by authored examples. |
| Any contiguous run longer than `^^^` | Legal, with the same meaning. |
| `^ ^` or `^/*gap*/^` | Invalid; trivia never joins two one-caret runs. |
| `^^ ^^` or `^^/*gap*/^^` on one logical statement | Invalid; the runs are not one four-caret break. |
| `^^` and `^^` separated by a real statement separator | Two break statements, never one deeper break. |
| `lhs ^ rhs` | Existing infix XOR syntax, unchanged. |
| Existing list forms such as `xs[^2]`, `xs[-: ^2]`, and `xs[?^ (...)]` | Existing list-caret syntax, unchanged. |

Every legal break spelling constructs exactly one `BreakAST` whose depth is
`1`, and lowering continues to construct exactly one `SGBreak` whose depth is
`1`. Caret count never selects an outer loop and is not preserved as runtime
state.

## Parser contract

The legacy statement entry in `src/StyioParser/Parser.cpp` and the nightly
statement entry in `src/StyioParser/NewParserExpr.cpp` implement the same
bounded operation:

1. Starting at the current `TOK_HAT`, count only immediately adjacent
   `TOK_HAT` tokens. Do not skip spaces, comments, line breaks, or semicolons
   while counting.
2. If the maximal contiguous run has length one, throw `StyioSyntaxError`
   before producing an AST. The stable diagnostic core is exactly:

   ```text
   break statement requires at least two consecutive '^' characters; use '^^' (minimum) or '^^^' (conventional)
   ```

   Both routes attach the normal source marker to the original caret. Repeated
   parses of the same labeled source must yield the same full diagnostic, and
   the legacy and nightly routes must agree on the stable core and location.
3. Otherwise consume exactly that maximal run and return
   `BreakAST::Create(1u)`.
4. Leave the following non-run token to the enclosing statement-boundary
   logic. A second same-line run is rejected rather than silently joined or
   accepted as an implicit adjacent statement.

This is a direct `O(k)` scan over a run of `k` carets with `O(1)` auxiliary
state. There is no run-length cap, allocation, cache, lexer token, or new
parser service. The two existing parser entries may keep the small operation
local; executable parity tests, rather than a new shared abstraction, prevent
semantic or diagnostic drift.

## Isolation invariants

- Run counting is reached only after statement dispatch has selected
  `TOK_HAT` as a break starter. It must not be moved into tokenization or a
  generic caret consumer.
- No operator table, conditional-expression parser, list-index parser, token
  enum, or lexer maximal-munch rule changes in this group.
- `BreakAST::getDepth()` remains `1` for `^^`, `^^^`, and representative long
  runs. No AST or IR field records source run length.
- Existing outside-loop rejection remains a semantic error for a legal break
  such as `^^^`; it must not be accidentally covered only by the new syntax
  error for `^`.
- Continue spelling and behavior are outside this capability.

## Complete migration inventory

All active authored single-caret break sites migrate to the conventional
`^^^` spelling in the same implementation closure. The repository audit must
include tracked and untracked active `.styio` files, while excluding generated
build trees.

Current in-repository sites are:

- `tests/features/control_flow/t06_inf_break.styio`;
- the single-caret site in
  `tests/features/control_flow/t08_multi_break.styio`;
- `tests/features/inferred_generics/t08_list_handle_lifetime.styio`; and
- `tests/features/stdio_input/t10_stdin_stream_break.styio`.

The intentional longer run already present in
`tests/features/control_flow/t08_multi_break.styio` remains the authored
long-run acceptance fixture; every other migrated example uses `^^^`.

The canonical downstream source
`../../SymPolicy/styio-example/brainfuck-compiler/src/brainfuck.styio` has
three single-caret break sites. All three migrate to `^^^`, and its existing
`make test` contract must pass against the newly built compiler. Positive C++
source fixtures that embed a break also migrate to `^^^`. A deliberately
invalid `^` remains only in the new syntax-diagnostic tests. The existing
outside-loop fixture uses `^^^` so it continues to prove the semantic boundary.

## Documentation single source of truth

`docs/design/Styio-EBNF.md` is the repository spelling SSOT. It records:

```ebnf
BREAK_TOKEN = '^' '^' { '^' } ;  (* length >= 2, contiguous, depth = 1 *)
break_stmt  = BREAK_TOKEN ;
```

`docs/design/Styio-Language-Design.md` explains the minimum, convention,
unbounded longer runs, nearest-loop normalization, and separated-run examples.
`docs/design/Styio-Symbol-Reference.md` is a compact index that points back to
the same contract. Neither secondary document may state that a single `^` is
legal or imply that count carries break depth.

## `design_pattern_assessment`

```text
pattern_catalog: refactoring-guru-catalog-22-v1
candidate: none
decision: reject
pressure: Two established parser statement entries must enforce one small lexical-context rule with deterministic parity, while adjacent caret meanings and single-layer IR semantics must remain isolated.
expected_benefit: No catalog pattern provides a verifiable benefit beyond the direct bounded token-run check plus parity tests; the acceptance seams themselves prevent drift.
simpler_alternative: Count the maximal contiguous TOK_HAT run at each existing statement entry, reject length one with the frozen message, and construct BreakAST::Create(1u) for every length >= 2. This is sufficient and keeps ownership visible.
application: Apply only the direct check in the legacy and nightly break branches. Preserve lexer, expression/list dispatch, AST, and IR boundaries; prove the result with route-parity, context-isolation, normalization, separated-run, migration, and downstream tests.
costs_and_rejections: A Strategy would invent interchangeable algorithms where there is one rule; Chain of Responsibility or State would obscure fixed statement dispatch; Adapter or Facade would add a wrapper around compatible parser contexts; a shared service solely for this check would add coupling and path ownership without eliminating meaningful complexity. Small duplicated control flow is accepted and locked by parity tests.
```

## Parallel frontier and ordered handoff

There is one ownership-coupled implementation Node after this design Node.
Parser parity, fixtures, source migration, and SSOT updates form one atomic
syntax closure, so splitting them would create a window in which the compiler
rejects still-active sources or documentation advertises the wrong grammar.
No artificial prerequisite is added to unrelated repository work.

The handoff order is:

1. **Design → implementation:** this document and `Validation.md` freeze the
   grammar matrix, exact diagnostic core, migration inventory, untouched
   contexts, and executable tests.
2. **Implementation → Verifier:** both parser routes, focused tests, all listed
   active sources, the downstream Brainfuck source, and all three language
   documents are changed as one closure. The handoff must contain no AST/IR or
   lexer change.
3. **Verifier → group Reviewer:** one focused regression proves parser parity,
   diagnostic determinism, separation boundaries, context isolation, and the
   migration audit.
4. **Reviewer → final validation:** after one complete review, run the impacted
   feature labels and Brainfuck downstream suite once and record evidence in
   `Evidence.md`.

## Predictive risks and blockers

- Counting after trivia skipping would incorrectly turn `^ ^` into `^^`.
- Testing only `^` outside a loop would confuse syntax rejection with the
  existing outside-loop semantic rejection; the two cases need distinct
  fixtures.
- A tracked-files-only scan would miss active untracked feature sources.
- Canonicalizing every long run would remove the only authored proof that
  lengths above three stay legal; the explicit `^^^^` fixture is retained.
- Updating only one parser route or asserting only AST type would miss
  diagnostic drift and accidental depth propagation.
- The downstream source is in a separate repository, so its source edit and
  test evidence must be handed off explicitly even though the compiler change
  is owned here.

There is no product or architecture decision blocker. The minimum, convention,
diagnostic guidance, normalization, and separated-run behavior are fully
determined by the requested contract.
