# Correct Syntax Contract

**Purpose:** Correct a Styio syntax contract when the compiler, tests, and language specification disagree about which form should be accepted or rejected.

**Last updated:** 2026-05-01

**TOML:** [CORRECT-SYNTAX-CONTRACT.toml](./CORRECT-SYNTAX-CONTRACT.toml) is the machine-readable workflow definition.

## When To Use

Use this workflow when a maintainer or user reports that a Styio spelling is accepted, rejected, or documented incorrectly. Typical triggers include:

1. A syntax form parses but should be impossible by design.
2. A new spelling is expected but currently falls through to the wrong parse or type error.
3. Parser behavior, Sema behavior, EBNF, symbol reference, and examples no longer agree.
4. The requested answer depends on an exact compiler diagnostic, not only a design explanation.

## Workflow

1. Reproduce the current behavior with the smallest `.styio` sample that demonstrates the disputed spelling.
2. Capture the exact compiler stage and diagnostic: lexer, parser, Sema/type inference, lowering, runtime, or docs-only mismatch.
3. Freeze the accepted spelling and rejected spelling in plain examples before editing implementation.
4. Inspect all syntax contract surfaces:
   - lexer/token names when tokens are involved
   - legacy parser
   - nightly parser
   - AST node shape and visitor path
   - Sema/type inference
   - lowering/runtime if execution semantics change
   - EBNF, language design, symbol reference, and examples
   - positive and negative tests
5. Decide the rejection boundary. Prefer parser errors for grammar-impossible forms and type errors for syntactically valid but semantically invalid forms.
6. Implement the narrowest parser/Sema/lowering change that preserves the existing AST and visitor architecture unless the syntax contract requires a new node.
7. Add or update positive tests for every accepted spelling and negative tests for every rejected spelling.
8. Update the language source of truth: compact examples, EBNF, symbol reference, team runbooks, test catalog, and generated indexes as relevant.
9. Re-run the disputed sample and record the exact diagnostic that users will see.
10. Run focused tests first, then docs gates and whitespace checks before reporting closure.

## Example Pattern

For conditional infinite loops, the rejected spelling was:

```styio
[...] ?(b > 0) >> {
    ...
}
```

The accepted spelling is:

```styio
[...] >> ?(b > 0) => {
    ...
}
```

The old spelling is rejected by the parser because `stream_source guard '>>' consumer` is not part of the grammar. A non-boolean guard in the accepted spelling is a Sema/type error instead.

## Required Evidence

1. Minimal reproduction for the disputed spelling.
2. Exact compiler diagnostic and exit stage for rejected syntax.
3. Positive parser or integration coverage for accepted syntax.
4. Negative parser coverage for grammar-impossible syntax.
5. Negative Sema coverage for syntactically valid but semantically invalid forms.
6. Runtime or reference-equivalence coverage when behavior changes execution output.
7. EBNF and syntax docs updated in the same delivery.
8. Owning team runbooks and `DOC-STATS.md` refreshed when docs gates require them.

## Gates

Use the build directory that exists for the current checkout. Replace `build/default` with the active build directory when necessary.

```bash
cmake --build build/default --target styio styio_test styio_security_test -j2
ctest --test-dir build/default -R '<focused syntax/security/runtime regex>' --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
git diff --check
```

When execution semantics are involved, also run the relevant golden, milestone, or reference-equivalence tests.

## Handoff

Report:

1. Rejected spelling and why it is rejected.
2. Accepted spelling and the owning grammar rule.
3. Exact compiler diagnostic for the rejected form.
4. Whether the language standard changed and which SSOT files changed.
5. Focused tests and docs gates that passed.
6. Any remaining parser-route, IDE grammar, or lowering work that was intentionally left out.
