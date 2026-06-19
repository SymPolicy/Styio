# Syntax Change Surface

## Code

1. Tokens: `src/StyioToken/Token.hpp`, `src/StyioToken/Token.cpp`
2. Lexer: `src/StyioParser/Tokenizer.cpp`
3. IDE lexer: `src/StyioServices/StyioIDE/Syntax.cpp`
4. Authoritative nightly parser: `src/StyioParser/Parser.cpp`, `src/StyioParser/NewParserExpr.cpp`
5. Retired parser-route audit evidence: `src/StyioParser/Parser.cpp`
6. AST shape: `src/StyioAST/AST.hpp`
7. Type/semantic checks: `src/StyioSema/TypeInfer.cpp`
8. StyioIR lowering: `src/StyioLowering/AstToStyioIR.cpp`

## Docs

1. Compact syntax page: `docs/design/syntax/`
2. EBNF: `docs/design/Styio-EBNF.md`
3. Symbol table: `docs/design/Styio-Symbol-Reference.md`
4. Semantic SSOT: `docs/design/Styio-Language-Design.md`
5. Team runbooks: `docs/teams/`

## Tests

1. Lexer/parser/security: `tests/security/styio_security_test.cpp`
2. Parser authority: `StyioDiagnostics.SyntaxCheckRejectsNonAuthoritativeParserEngine`
3. Shadow gates: `parser_shadow_gate_*` zero accepted-grammar fallback evidence
4. Runtime smoke: `build/default/bin/styio --file <sample>`
