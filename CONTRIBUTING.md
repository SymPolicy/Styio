# Contributing

Styio changes should be small, testable, and tied to the compiler, language
semantics, CLI contract, or repository documentation owned by this checkout.

Before sending a change:

- Build the touched targets with `cmake --build build/default`.
- Run the closest CTest labels for the change.
- Run `python3 scripts/docs-index.py --check` after documentation tree edits.
- Run `python3 scripts/docs-audit.py` for public documentation changes.
- Run `git diff --check` before committing.

Use the existing parser, sema, lowering, and test patterns unless the change is
explicitly about replacing them. Do not introduce public positioning, product
comparisons, or benchmark conclusions without repository-local evidence and a
linked reproducible gate.
