# Keyword-Free Lexical Contract

**Purpose:** Own the invariant that Styio has no word-token keywords and that exact word spellings gain meaning only inside a symbol-anchored structural context.

**Last updated:** 2026-07-30

## Feature Contract

```toml syntax-feature
schema_version = 1
id = "core.keyword-free-lexical-contract"
title = "Keyword-Free Lexical Contract"
kind = "lexical-contract"
decision_state = "accepted"
delivery_state = "converged"
owner = "Parser / Grammar"
syntax = "Every identifier-shaped word is tokenized as NAME; punctuation and surrounding structure select language forms."
resolution = "Styio reserves no word token. Parsers may inspect an exact NAME spelling only after a symbol-anchored production has selected that context."
golden_cases = ["tests/features/keyword_free/t01_word_identifiers.styio", "tests/security/styio_security_test.cpp"]

[documents]
grammar = ["docs/design/Styio-EBNF.md"]
tokens = ["docs/design/Styio-Symbol-Reference.md"]
semantics = ["docs/design/Styio-Language-Design.md"]
diagnostics = ["workflows/TEST-CATALOG.md"]
compatibility = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
teaching = ["docs/design/syntax/ACTIVE-SYNTAX.md"]
implementation = ["src/StyioParser/Tokenizer.cpp"]
evidence = ["tests/features/keyword_free/t01_word_identifiers.styio", "tests/security/styio_security_test.cpp"]

[prerequisites]
language-owner-approval = "docs/specs/AGENT-SPEC.md"
keyword-free-contract = "docs/design/syntax/features/core-keyword-free-lexical-contract.md"
nightly-parser-authority = "docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md"
grammar-contract = "docs/design/Styio-EBNF.md"
semantic-contract = "docs/design/Styio-Language-Design.md"
diagnostic-boundary = "workflows/TEST-CATALOG.md"
compatibility-decision = "docs/design/syntax/ACTIVE-SYNTAX.md"
golden-evidence = "tests/features/keyword_free/t01_word_identifiers.styio"

[implementation]
path = "src/StyioParser/Tokenizer.cpp"
symbol = "StyioTokenizer::tokenize"
owner = "Parser / Grammar"

[dependencies]
requires = []
requires_any = []
extends = []
conflicts = []
supersedes = []
after = []
```

## Decision

The tokenizer emits `NAME` for every identifier-shaped word, including
spellings that other languages commonly reserve. The parser may inspect an
exact spelling only after punctuation or another already-selected structural
production gives that spelling a local role. A bare word never selects a
grammar branch by itself.

`true` and `false` follow the same lexical rule and therefore remain `NAME`
tokens. In expression-literal context their exact spelling denotes a Boolean
literal; this feature does not decide whether Boolean literal spellings may be
shadowed.

Word-headed `schema` syntax is retired. The word `schema` is an ordinary name
outside any future symbol-anchored schema production.

## Diagnostic Boundary

Diagnostics describe the missing or invalid symbol structure, not a
"reserved keyword." Tooling must not publish a separate keyword token class or
infer a word-headed production that the compiler does not own.

## Compatibility Boundary

Existing symbol-anchored exact-name contexts such as `@ extern(...)` remain
valid because `@` selects the native-interop production before the parser
checks the following `NAME`. Adding any new exact-name context requires its own
feature decision.

## Evolution Boundary

A future contextual literal, prelude identity, or named capability must extend
this feature and state its anchor, shadowing behavior, and diagnostic boundary.
Introducing a word-token keyword conflicts with this feature and requires an
explicit replacement decision.
