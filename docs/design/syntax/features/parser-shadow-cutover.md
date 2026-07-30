# Parser Shadow Cutover

**Purpose:** Own the single accepted-parser authority, shadow accounting, fallback prohibition, and parser-route convergence evidence.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "parser.shadow-cutover"
title = "Parser Shadow Cutover"
kind = "parser-governance"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Shadow Gates"
syntax = "nightly parser primary path with retained parser routes used only for explicit audit evidence"
resolution = "Keep the hand-written nightly compiler parser as the only accepted grammar authority and require zero accepted-grammar fallback."
golden_cases = ["tests/features/scalar_expressions/t01_int_arith.styio", "tests/features/functions/t01_simple_func.styio"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["workflows/PROMOTE-NIGHTLY-PARSER-SUBSET.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Parser.cpp"]
evidence = ["tests/features/scalar_expressions/t01_int_arith.styio", "tests/features/functions/t01_simple_func.styio", "benchmark/parser-shadow-suite-gate.py"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/Styio-Language-Design.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "workflows/PROMOTE-NIGHTLY-PARSER-SUBSET.md"
golden-evidence = "tests/features/scalar_expressions/t01_int_arith.styio"
shadow-accounting = "benchmark/parser-shadow-suite-gate.py"

[implementation]
path = "src/StyioParser/Parser.cpp"
symbol = "parse_main_block_shadow_nightly"
owner = "Parser / Shadow Gates"

[dependencies]
requires = []
requires_any = []
extends = []
conflicts = []
supersedes = []
after = []
```

## Decision

Only the nightly compiler parser decides accepted Styio grammar. Retained parser
routes may supply comparisons, migration counters, and negative evidence, but
they cannot rescue an accepted form through fallback.

## Evolution Boundary

Parser replacement, generated grammar authority, recovery-mode promotion, or
fallback reintroduction requires a new parser-authority decision and
revalidation of every active feature document.
