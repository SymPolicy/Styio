# Minimum Break Spelling Validation

**Purpose:** Define validation coverage for standalone break spelling behavior.

**Last updated:** 2026-08-02

## Acceptance matrix

The implementation adds focused parser tests whose names contain
`BreakSpelling`. Each applicable row runs through both the legacy and nightly
statement routes.

| Case | Required proof |
| --- | --- |
| `^` at top level and inside a loop block | `StyioSyntaxError`; no AST; exact stable diagnostic core; marker points at the original caret. |
| The same `^` parsed twice | Byte-for-byte identical full diagnostic. |
| The same `^` through both routes | Matching stable core and source location. |
| `^^` | One `BreakAST`, `getDepth() == 1`, and complete run consumption. |
| `^^^` | Same result as `^^`; documented conventional spelling. |
| `^^^^` and a representative long run (at least 64 carets) | One `BreakAST`, depth `1`, no arbitrary upper bound. |
| `^ ^` and `^/*gap*/^` | Rejected at the first one-caret run with the minimum-length diagnostic; never merged. |
| `^^ ^^` and `^^/*gap*/^^` on one logical statement | Rejected as separated runs; never normalized as a four-caret break. |
| `^^\n^^` (or the repository's explicit statement separator) | Two depth-`1` break ASTs, not one depth-`2` break. |
| Legal break spellings lowered through the normal pipeline | Exactly one `SGBreak` with depth `1`; nearest enclosing loop behavior is unchanged. |
| Legal `^^^` outside a loop | Existing `break outside enclosing loop` semantic failure, proving syntax and semantic checks remain distinct. |

The context-isolation test, also named with `BreakSpelling`, retains the
established assertions for infix XOR and list-caret forms. It must prove their
existing AST/operator kinds and must not route them through the break-run
check. This test is a non-regression seam, not a request to redesign or extend
either expression parser.

## Migration assertions

After migration, the only isolated single `^` in active `.styio` input is none:
all current isolated occurrences are authored breaks and therefore become
`^^^`. The scan deliberately includes untracked active files and the canonical
Brainfuck source, and excludes generated build output. Run it from the
`styio-nightly` repository root:

```sh
set +e
rg -n --pcre2 '(?<!\^)\^(?!\^)' \
  --glob '*.styio' \
  --glob '!build/**' \
  --glob '!cmake-build-*/**' \
  . ../../SymPolicy/styio-example/brainfuck-compiler/src/brainfuck.styio
break_spelling_scan_status=$?
set -e
test "$break_spelling_scan_status" -eq 1
```

This is a one-time completeness audit of the current migration inventory, not
a permanent ban on the valid infix/list syntax. If a legitimate isolated caret
is added to an active `.styio` source before this group lands, classify it with
the context-isolation contract instead of rewriting it as a break.

The authoritative document review must establish all of the following:

- `docs/design/Styio-EBNF.md` contains `BREAK_TOKEN = '^' '^' { '^' }` and
  says length is at least two, contiguous, and normalized to depth one;
- `docs/design/Styio-Language-Design.md` shows `^` only as an invalid example,
  `^^` as the minimum, `^^^` as conventional, and longer runs as valid;
- `docs/design/Styio-Symbol-Reference.md` does not list bare `^` as a legal
  break spelling; and
- all three documents reject separated same-statement runs and preserve
  nearest-loop semantics.

## Focused Node regression

Build once after the implementation and Verifier repairs are complete, then
run only the focused parser contract and migration audit:

```sh
cmake --build build -j2 --target styio_parser_internal_test styio_security_test styio
ctest --test-dir build -R 'BreakSpelling' --output-on-failure

set +e
rg -n --pcre2 '(?<!\^)\^(?!\^)' \
  --glob '*.styio' \
  --glob '!build/**' \
  --glob '!cmake-build-*/**' \
  . ../../SymPolicy/styio-example/brainfuck-compiler/src/brainfuck.styio
break_spelling_scan_status=$?
set -e
test "$break_spelling_scan_status" -eq 1
```

Focused success requires:

- both parser routes pass every syntax-matrix row;
- their single-caret diagnostic core and marker agree;
- XOR/list caret tests remain unchanged in meaning;
- valid runs always normalize to AST and IR depth one; and
- no active authored single-caret break remains.

Do not run the impacted feature labels or Brainfuck suite between Worker and
Verifier; those belong to the single group-level regression below.

## One group-level regression

After the one group Reviewer completes, run this impacted regression exactly
once:

```sh
cmake --build build -j2 --target styio styio_parser_internal_test styio_security_test
ctest --test-dir build -R 'BreakSpelling' --output-on-failure
ctest --test-dir build -L control_flow --output-on-failure
ctest --test-dir build -L stdio_input --output-on-failure
ctest --test-dir build -L inferred_generics --output-on-failure
STYIO_BIN="$PWD/build/bin/styio" \
  make -C ../../SymPolicy/styio-example/brainfuck-compiler test
git diff --check
```

Group acceptance requires all commands to succeed and the Reviewer to confirm
that production changes are confined to the two existing statement parser
entries. Tokenization, infix/list caret ownership, AST shape, Styio IR shape,
lowering, and code generation must have no implementation diff for this
capability.

## Evidence handoff

`docs/plan/roadmap/break-statement-spelling/Evidence.md` records:

1. the focused test names and passing result;
2. the identical legacy/nightly diagnostic core;
3. representative AST/IR depth-one assertions for `^^`, `^^^`, and the long
   run;
4. the isolated-caret audit result, including the downstream source;
5. the three document checks; and
6. the single final feature-label and Brainfuck regression result.

No evidence is accepted from a test that only checks successful parsing or
only checks source text: normalization, parser parity, negative separated-run
behavior, and downstream execution are independent acceptance seams.
