# Add Resource Identifier

**Purpose:** Add or change a Styio resource identifier without drifting syntax, lifecycle, docs, and tests.

**Last updated:** 2026-05-04

**TOML:** [ADD-RESOURCE-IDENTIFIER.toml](./ADD-RESOURCE-IDENTIFIER.toml) is the machine-readable workflow definition.

## Skill

Use [styio-resource-identifier-change/skill.toml](./skills/styio-resource-identifier-change/skill.toml) for `@name` resource forms and built-in resource symbols.

## Workflow

1. Add or update the internal Styio declaration, for example
   `@ name : type := #(args) => { body }`.
2. Record accepted source forms, capability, direction, ownership, lifetime, and close/error behavior.
3. Update syntax docs, prelude source, and lifecycle docs before implementation details drift.
4. Update parser, analyzer, runtime, or config surfaces only where the declaration needs substrate behavior.
5. Add positive and fail-closed tests.

## Required Evidence

1. Internal Styio resource declaration and accepted source forms.
2. Capability and lifetime statement.
3. Parser or analyzer coverage.
4. Runtime or fail-closed coverage.
5. Docs updated in the resource identifier SSOT.

## Constraints

Parameterized resource expressions are not declaration heads. Invalid example:
`@name{arg} := { ... }`. Do not introduce hidden pseudo-primitives such as `file(path)`; resources
must be defined in Styio prelude source, with C++ limited to parser/runtime substrate behavior.

## Gates

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -L security --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
git diff --check
```

## Handoff

Report the internal declaration, accepted identifiers, reserved identifiers, lifecycle behavior,
tests, and unsupported paths.
