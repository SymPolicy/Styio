# Add Syntax With Skills

**Purpose:** Orchestrate new Styio syntax work through repo-local skills, implementation surfaces, docs, and gates.

**Last updated:** 2026-04-23

**TOML:** [ADD-SYNTAX-WITH-SKILLS.toml](./ADD-SYNTAX-WITH-SKILLS.toml) is the machine-readable workflow definition.

## Skill

Use [styio-syntax-change/skill.toml](./skills/styio-syntax-change/skill.toml) when the request touches syntax, operators, tokens, grammar, parser behavior, IDE highlighting, semantics, or syntax docs/tests.

## Workflow

1. Load the skill and its surface map.
2. Freeze the accepted examples and reserved symbols.
3. Implement in this order:
   - token enum/name/lexer
   - IDE tokenizer
   - legacy parser
   - nightly parser
   - AST/type/IR behavior
   - docs and runbooks
   - tests
4. If runtime lowering is incomplete, reject the accepted-looking form with a typed fail-closed diagnostic.
5. Prove parity and boundaries before reporting completion.

## Required Evidence

1. Lexer coverage for every new token.
2. Legacy and nightly parser coverage for every accepted form.
3. Semantic negative coverage for every accepted-but-not-lowered form.
4. Runtime smoke for every supported lowering path.
5. Docs updated in compact syntax, EBNF, symbol reference, and semantic SSOT.

## Gates

```bash
cmake --build build/default --target styio_security_test styio -j2
ctest --test-dir build/default -L security --output-on-failure
ctest --test-dir build/default -R '^StyioParserEngine\.' --output-on-failure
ctest --test-dir build/default -R '^parser_shadow_gate_' --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
git diff --check
```

## Handoff

Report accepted syntax, reserved syntax, unsupported lowering paths, docs touched, exact gates run, and remaining lifecycle or runtime work.
