# Add Resource Identifier

**Purpose:** Mirror the root workflow for adding or changing Styio resource identifiers.

**Last updated:** 2026-05-04

Canonical workflow: [../../../workflows/ADD-RESOURCE-IDENTIFIER.md](../../../workflows/ADD-RESOURCE-IDENTIFIER.md)

## Workflow

1. Add or update the internal Styio declaration, for example
   `@ name : type := #(args) => { body }`.
2. Record accepted source forms, capability, direction, ownership, lifetime, and error behavior.
3. Update resource syntax, prelude source, and lifecycle docs.
4. Update implementation surfaces only where substrate behavior is needed.
5. Add positive and fail-closed tests.

Parameterized resource expressions are not declaration heads. Invalid example:
`@name{arg} := { ... }`. Do not introduce hidden pseudo-primitives such as `file(path)`.

## Gates

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -L security --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
git diff --check
```
