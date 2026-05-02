# Add Resource Identifier

**Purpose:** Add or change a Styio resource identifier without drifting syntax, lifecycle, docs, and tests.

**Last updated:** 2026-04-23

**TOML:** [ADD-RESOURCE-IDENTIFIER.toml](./ADD-RESOURCE-IDENTIFIER.toml) is the machine-readable workflow definition.

## Skill

Use [styio-resource-identifier-change/skill.toml](./skills/styio-resource-identifier-change/skill.toml) for `@name` resource forms and built-in resource symbols.

## Workflow

1. Define the resource as a compact built-in symbol, for example `@stdin := { ... }`.
2. Record capability, direction, ownership, lifetime, and close/error behavior.
3. Update syntax docs and lifecycle docs before implementation details drift.
4. Update parser, analyzer, runtime, or config surfaces only where the identifier needs behavior.
5. Add positive and fail-closed tests.

## Required Evidence

1. Built-in symbol definition.
2. Capability and lifetime statement.
3. Parser or analyzer coverage.
4. Runtime or fail-closed coverage.
5. Docs updated in the resource identifier SSOT.

## Gates

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -L security --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
git diff --check
```

## Handoff

Report accepted identifiers, reserved identifiers, lifecycle behavior, tests, and unsupported paths.
