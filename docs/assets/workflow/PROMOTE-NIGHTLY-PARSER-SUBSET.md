# Promote Nightly Parser Subset

**Purpose:** Mirror the root workflow for moving grammar coverage into the nightly parser.

**Last updated:** 2026-04-23

Canonical workflow: [../../../workflows/PROMOTE-NIGHTLY-PARSER-SUBSET.md](../../../workflows/PROMOTE-NIGHTLY-PARSER-SUBSET.md)

## Workflow

1. Freeze accepted and rejected samples.
2. Extend token and start gates.
3. Implement nightly expr or stmt parsing.
4. Preserve line and statement boundaries.
5. Add parity, fallback-budget, and error-boundary tests.

## Gates

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -R '^StyioParserEngine\.' --output-on-failure
ctest --test-dir build/default -R '^parser_shadow_gate_' --output-on-failure
ctest --test-dir build/default -R '^parser_legacy_entry_audit$' --output-on-failure
git diff --check
```
