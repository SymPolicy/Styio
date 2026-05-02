# Promote Nightly Parser Subset

**Purpose:** Move a grammar slice into the nightly parser while keeping legacy parity and shadow gates intact.

**Last updated:** 2026-04-23

**TOML:** [PROMOTE-NIGHTLY-PARSER-SUBSET.toml](./PROMOTE-NIGHTLY-PARSER-SUBSET.toml) is the machine-readable workflow definition.

## Skill

Use [styio-parser-subset/skill.toml](./skills/styio-parser-subset/skill.toml) when advancing nightly parser coverage.

## Workflow

1. Freeze representative source samples and expected legacy behavior.
2. Extend token/start gates before parser consumption.
3. Implement nightly expr or stmt parsing with existing helpers.
4. Preserve line-boundary and statement-boundary behavior.
5. Add AST repr parity, fallback budget, and error-boundary tests.

## Required Evidence

1. Legacy and nightly AST repr match for accepted samples.
2. Rejected samples fail with stable parser or semantic errors.
3. Shadow route stats do not regress the relevant fallback budget.
4. Parser entry audit still passes.

## Gates

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -R '^StyioParserEngine\.' --output-on-failure
ctest --test-dir build/default -R '^parser_shadow_gate_' --output-on-failure
ctest --test-dir build/default -R '^parser_legacy_entry_audit$' --output-on-failure
git diff --check
```

## Handoff

Report accepted samples, rejected samples, fallback/internal-bridge budget, and changed parser surfaces.
