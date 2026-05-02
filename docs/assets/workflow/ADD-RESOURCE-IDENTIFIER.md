# Add Resource Identifier

**Purpose:** Mirror the root workflow for adding or changing Styio resource identifiers.

**Last updated:** 2026-04-23

Canonical workflow: [../../../workflows/ADD-RESOURCE-IDENTIFIER.md](../../../workflows/ADD-RESOURCE-IDENTIFIER.md)

## Workflow

1. Define the resource as `@name := { ... }`.
2. Record capability, direction, ownership, lifetime, and error behavior.
3. Update resource syntax and lifecycle docs.
4. Update implementation surfaces only where behavior is needed.
5. Add positive and fail-closed tests.

## Gates

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -L security --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
git diff --check
```
