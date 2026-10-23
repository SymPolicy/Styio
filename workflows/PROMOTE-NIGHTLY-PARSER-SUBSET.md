# Promote Nightly Parser Coverage

**Purpose:** Move grammar coverage into the authoritative nightly parser while keeping accepted syntax no-fallback.

**Last updated:** 2026-05-20

**TOML:** [PROMOTE-NIGHTLY-PARSER-SUBSET.toml](./PROMOTE-NIGHTLY-PARSER-SUBSET.toml) is the machine-readable workflow definition.

## Skill

Use [styio-parser-subset/skill.toml](./skills/styio-parser-subset/skill.toml) when advancing authoritative nightly parser coverage.

## Workflow

1. Freeze representative accepted and rejected source samples.
2. Extend token/start gates before parser consumption.
3. Implement nightly expr or stmt parsing with existing helpers.
4. Preserve line-boundary and statement-boundary behavior.
5. Add no-fallback, parser-entry, and error-boundary tests.

## Required Evidence

1. Accepted samples parse through the hand-written nightly parser without fallback.
2. Rejected samples fail with stable parser or semantic errors.
3. Shadow route stats report zero accepted-grammar fallback.
4. Parser entry audit still passes.

## Gates

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -R '^StyioParserEngine\.' --output-on-failure
ctest --test-dir build/default -R '^parser_shadow_gate_' --output-on-failure
ctest --test-dir build/default -R '^parser_legacy_entry_audit$' --output-on-failure
ctest --test-dir build/default -R '^(StyioDiagnostics\.SyntaxCheckRejectsNonAuthoritativeParserEngine|StyioSemanticBridge\.RejectsMalformedInputWithoutRecovery)$' --output-on-failure
git diff --check
```

## Handoff

Report accepted samples, rejected samples, no-fallback evidence, and changed parser surfaces.
